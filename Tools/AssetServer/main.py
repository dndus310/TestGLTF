"""
Asset server (closed-network friendly)
- Upload / list / info / download / delete user assets (.glb / .gltf)
- Files on local disk (./storage), metadata in SQLite (./assets.db)
- Swagger UI served from ./static/swagger (no CDN access needed)

Run:  .venv\\Scripts\\python -m uvicorn main:app --host 0.0.0.0 --port 8000
Docs: http://localhost:8000/docs
"""
import base64
import binascii
import hashlib
import json
import sqlite3
import struct
import uuid
from datetime import datetime, timezone
from pathlib import Path

from fastapi import FastAPI, File, HTTPException, Query, Request, UploadFile
from pydantic import BaseModel, Field
from fastapi.openapi.docs import get_swagger_ui_html
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

BASE = Path(__file__).resolve().parent
STORE = BASE / "storage"
DB = BASE / "assets.db"
MAX_SIZE = 500 * 1024 * 1024  # 500 MB
CHUNK = 1024 * 1024
MAX_JSON_MB = 20  # base64-in-JSON is only for small files (+33% size, whole file in memory)
MAX_JSON_B64 = (MAX_JSON_MB * 1024 * 1024 * 4) // 3 + 4
STORE.mkdir(exist_ok=True)

app = FastAPI(title="Asset Server", version="0.1.0", docs_url=None, redoc_url=None)
app.mount("/static", StaticFiles(directory=BASE / "static"), name="static")


def custom_openapi():
    # FastAPI emits OAS 3.1 `contentMediaType` for UploadFile; add `format: binary` so the
    # bundled Swagger UI renders a file picker instead of a text box.
    if app.openapi_schema:
        return app.openapi_schema
    from fastapi.openapi.utils import get_openapi
    schema = get_openapi(title=app.title, version=app.version, routes=app.routes)

    def patch(node):
        if isinstance(node, dict):
            if node.get("type") == "string" and node.get("contentMediaType") == "application/octet-stream":
                node["format"] = "binary"
            for v in node.values():
                patch(v)
        elif isinstance(node, list):
            for v in node:
                patch(v)

    patch(schema)
    app.openapi_schema = schema
    return schema


app.openapi = custom_openapi


@app.get("/docs", include_in_schema=False)
def swagger_ui():
    # Self-hosted Swagger UI: works without internet (default /docs loads from a CDN)
    return get_swagger_ui_html(
        openapi_url=app.openapi_url,
        title=app.title + " - Swagger UI",
        swagger_js_url="/static/swagger/swagger-ui-bundle.js",
        swagger_css_url="/static/swagger/swagger-ui.css",
        swagger_favicon_url="data:,",
    )


# ---------------------------------------------------------------- DB
def db():
    con = sqlite3.connect(DB)
    con.row_factory = sqlite3.Row
    return con


with db() as con:
    con.execute("""CREATE TABLE IF NOT EXISTS assets (
        id TEXT PRIMARY KEY, name TEXT NOT NULL, ext TEXT NOT NULL, size INTEGER NOT NULL,
        sha256 TEXT NOT NULL, created_at TEXT NOT NULL, summary TEXT)""")


def row_to_dict(r):
    d = dict(r)
    d["summary"] = json.loads(d["summary"]) if d.get("summary") else None
    d["download_url"] = f"/assets/{d['id']}/download"
    return d


def get_row(asset_id: str):
    with db() as con:
        r = con.execute("SELECT * FROM assets WHERE id=?", (asset_id,)).fetchone()
    if not r:
        raise HTTPException(404, "asset not found")
    return r


# ---------------------------------------------------------------- GLB check
def inspect_glb(path: Path) -> dict:
    """Validate GLB header/JSON chunk and return a small summary."""
    with path.open("rb") as f:
        head = f.read(20)
        if len(head) < 20:
            raise ValueError("file too small")
        magic, version, length = struct.unpack_from("<4sII", head, 0)
        if magic != b"glTF":
            raise ValueError("not a GLB (magic mismatch)")
        if version != 2:
            raise ValueError(f"unsupported glTF version {version}")
        if length != path.stat().st_size:
            raise ValueError("GLB length field does not match file size")
        jlen, jtype = struct.unpack_from("<I4s", head, 12)
        if jtype != b"JSON":
            raise ValueError("first chunk is not JSON")
        gj = json.loads(f.read(jlen))
    return {
        "generator": gj.get("asset", {}).get("generator"),
        "extensionsUsed": gj.get("extensionsUsed", []),
        **{k: len(gj.get(k, [])) for k in ("nodes", "meshes", "materials", "textures", "images", "skins", "animations")},
    }


# ---------------------------------------------------------------- API
async def save_asset(filename: str | None, chunks) -> dict:
    """Common save path for every upload style. `chunks` is an async iterator of bytes."""
    name = Path(filename or "").name
    ext = Path(name).suffix.lower()
    if ext not in (".glb", ".gltf"):
        raise HTTPException(400, "only .glb / .gltf files are allowed")

    asset_id = str(uuid.uuid4())
    dst = STORE / f"{asset_id}{ext}"
    sha, size = hashlib.sha256(), 0
    try:
        with dst.open("wb") as out:
            async for chunk in chunks:  # stream to disk, don't hold it all in memory
                size += len(chunk)
                if size > MAX_SIZE:
                    raise HTTPException(413, f"file larger than {MAX_SIZE // (1024 * 1024)} MB")
                sha.update(chunk)
                out.write(chunk)
        if size == 0:
            raise HTTPException(400, "empty body")
        summary = None
        if ext == ".glb":
            try:
                summary = inspect_glb(dst)
            except ValueError as e:
                raise HTTPException(400, f"invalid GLB: {e}")
    except BaseException:
        dst.unlink(missing_ok=True)
        raise

    rec = (asset_id, name, ext, size, sha.hexdigest(), datetime.now(timezone.utc).isoformat(),
           json.dumps(summary) if summary else None)
    with db() as con:
        con.execute("INSERT INTO assets VALUES (?,?,?,?,?,?,?)", rec)
    return row_to_dict(get_row(asset_id))


async def _upload_chunks(file: UploadFile):
    while chunk := await file.read(CHUNK):
        yield chunk


async def _one_chunk(data: bytes):
    yield data


@app.post("/assets", summary="Upload asset - multipart/form-data (Swagger, curl)", status_code=201)
async def upload_asset(file: UploadFile = File(..., description="GLB or glTF file")):
    return await save_asset(file.filename, _upload_chunks(file))


@app.post("/assets/raw", summary="Upload asset - raw binary body (recommended for UE HttpModule)", status_code=201,
          openapi_extra={"requestBody": {"required": True, "content": {
              "application/octet-stream": {"schema": {"type": "string", "format": "binary"}}}}})
async def upload_asset_raw(request: Request,
                           name: str = Query(..., description="original file name, e.g. Gun.glb")):
    # Body = the file bytes as-is (Content-Type: application/octet-stream). Response = JSON.
    return await save_asset(name, request.stream())


class Base64Upload(BaseModel):
    name: str = Field(..., examples=["Gun.glb"])
    data_base64: str = Field(..., description="file bytes, base64-encoded")


@app.post("/assets/json", summary="Upload asset - JSON with base64 data (small files only)", status_code=201)
async def upload_asset_json(body: Base64Upload):
    if len(body.data_base64) > MAX_JSON_B64:
        raise HTTPException(413, f"base64 payload too large; use /assets/raw for files over {MAX_JSON_MB} MB")
    try:
        data = base64.b64decode(body.data_base64, validate=True)
    except (binascii.Error, ValueError):
        raise HTTPException(400, "data_base64 is not valid base64")
    return await save_asset(body.name, _one_chunk(data))


@app.get("/assets", summary="List assets")
def list_assets(q: str | None = Query(None, description="filter by file name"),
                limit: int = Query(100, ge=1, le=1000), offset: int = Query(0, ge=0)):
    sql, args = "SELECT * FROM assets", []
    if q:
        sql += " WHERE name LIKE ?"
        args.append(f"%{q}%")
    sql += " ORDER BY created_at DESC LIMIT ? OFFSET ?"
    args += [limit, offset]
    with db() as con:
        rows = con.execute(sql, args).fetchall()
    return [row_to_dict(r) for r in rows]


@app.get("/assets/{asset_id}", summary="Asset info")
def asset_info(asset_id: str):
    return row_to_dict(get_row(asset_id))


@app.get("/assets/{asset_id}/download", summary="Download asset file")
def download_asset(asset_id: str):
    r = get_row(asset_id)
    path = STORE / f"{r['id']}{r['ext']}"
    if not path.exists():
        raise HTTPException(410, "file missing from storage")
    media = "model/gltf-binary" if r["ext"] == ".glb" else "model/gltf+json"
    return FileResponse(path, media_type=media, filename=r["name"], headers={"ETag": r["sha256"]})


@app.get("/assets/{asset_id}/base64", summary="Download asset as JSON with base64 data (small files only)")
def download_asset_base64(asset_id: str):
    r = get_row(asset_id)
    if r["size"] > MAX_JSON_MB * 1024 * 1024:
        raise HTTPException(413, f"file over {MAX_JSON_MB} MB; use /assets/{asset_id}/download")
    path = STORE / f"{r['id']}{r['ext']}"
    if not path.exists():
        raise HTTPException(410, "file missing from storage")
    return {**row_to_dict(r), "data_base64": base64.b64encode(path.read_bytes()).decode("ascii")}


@app.delete("/assets/{asset_id}", summary="Delete asset")
def delete_asset(asset_id: str):
    r = get_row(asset_id)
    (STORE / f"{r['id']}{r['ext']}").unlink(missing_ok=True)
    with db() as con:
        con.execute("DELETE FROM assets WHERE id=?", (asset_id,))
    return {"deleted": asset_id}


@app.get("/health", include_in_schema=False)
def health():
    return {"ok": True}
