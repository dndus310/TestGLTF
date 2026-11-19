"""Minimal GLB structural check + summary (no dependencies)."""
import json, struct, sys

path = sys.argv[1]
data = open(path, "rb").read()
magic, version, length = struct.unpack_from("<4sII", data, 0)
assert magic == b"glTF", "not a GLB"
assert length == len(data), "length mismatch %d != %d" % (length, len(data))
off, chunks = 12, []
while off < len(data):
    clen, ctype = struct.unpack_from("<I4s", data, off)
    chunks.append((ctype, data[off + 8: off + 8 + clen]))
    off += 8 + clen
gj = json.loads(chunks[0][1])
binlen = len(chunks[1][1]) if len(chunks) > 1 else 0

print("file      :", path, "(%d bytes, glTF v%d)" % (len(data), version))
print("generator :", gj.get("asset", {}).get("generator"))
print("extensions:", gj.get("extensionsUsed", []))
print("BIN chunk :", binlen, "bytes")
for k in ("scenes", "nodes", "meshes", "materials", "textures", "images", "skins", "animations"):
    print("%-10s: %d" % (k, len(gj.get(k, []))))
acc = gj.get("accessors", [])
for i, m in enumerate(gj.get("meshes", [])):
    for j, p in enumerate(m["primitives"]):
        a = p["attributes"]
        pos = acc[a["POSITION"]]
        idx = acc[p["indices"]]["count"] if "indices" in p else None
        print("mesh[%d].prim[%d]: attrs=%s verts=%d indices=%s tris=%s material=%s" % (
            i, j, sorted(a), pos["count"], idx, idx // 3 if idx else None, p.get("material")))
        print("   bounds min=%s max=%s" % ([round(v, 3) for v in pos.get("min", [])], [round(v, 3) for v in pos.get("max", [])]))
for i, mat in enumerate(gj.get("materials", [])):
    pbr = mat.get("pbrMetallicRoughness", {})
    print("material[%d] %s: baseColorTex=%s normalTex=%s metallic=%s roughness=%s" % (
        i, mat.get("name"), "baseColorTexture" in pbr, "normalTexture" in mat,
        pbr.get("metallicFactor", 1.0), pbr.get("roughnessFactor", 1.0)))
for i, img in enumerate(gj.get("images", [])):
    bv = gj["bufferViews"][img["bufferView"]] if "bufferView" in img else None
    print("image[%d] %s %s %s bytes" % (i, img.get("name"), img.get("mimeType"), bv["byteLength"] if bv else img.get("uri")))
# world-space bounds through the node hierarchy (TRS or matrix)
def mat_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]

def local_mat(n):
    if "matrix" in n:
        m = n["matrix"]  # column-major
        return [[m[c * 4 + r] for c in range(4)] for r in range(4)]
    tx, ty, tz = n.get("translation", [0, 0, 0])
    x, y, z, w = n.get("rotation", [0, 0, 0, 1])
    sx, sy, sz = n.get("scale", [1, 1, 1])
    r = [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
         [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
         [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]
    return [[r[0][0] * sx, r[0][1] * sy, r[0][2] * sz, tx],
            [r[1][0] * sx, r[1][1] * sy, r[1][2] * sz, ty],
            [r[2][0] * sx, r[2][1] * sy, r[2][2] * sz, tz],
            [0, 0, 0, 1]]

nodes = gj.get("nodes", [])
wmin, wmax = [1e30] * 3, [-1e30] * 3
def walk(i, parent):
    global wmin, wmax
    n = nodes[i]
    m = mat_mul(parent, local_mat(n))
    if "mesh" in n:
        for p in gj["meshes"][n["mesh"]]["primitives"]:
            a = acc[p["attributes"]["POSITION"]]
            lo, hi = a["min"], a["max"]
            for cx in (lo[0], hi[0]):
                for cy in (lo[1], hi[1]):
                    for cz in (lo[2], hi[2]):
                        v = [m[r][0] * cx + m[r][1] * cy + m[r][2] * cz + m[r][3] for r in range(3)]
                        wmin = [min(wmin[k], v[k]) for k in range(3)]
                        wmax = [max(wmax[k], v[k]) for k in range(3)]
    for c in n.get("children", []):
        walk(c, m)
I4 = [[1 if r == c else 0 for c in range(4)] for r in range(4)]
for root in gj["scenes"][gj.get("scene", 0)]["nodes"]:
    walk(root, I4)
if wmin[0] < 1e29:
    size = [round(wmax[k] - wmin[k], 4) for k in range(3)]
    print("world bbox: min=%s max=%s size(m, Y-up)=%s" % ([round(v, 4) for v in wmin], [round(v, 4) for v in wmax], size))

# bufferView bounds check
for i, bv in enumerate(gj.get("bufferViews", [])):
    assert bv.get("byteOffset", 0) + bv["byteLength"] <= binlen, "bufferView %d out of range" % i
print("structure : OK")
