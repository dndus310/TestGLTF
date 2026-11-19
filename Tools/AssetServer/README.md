# Asset Server

사용자 에셋(.glb / .gltf)을 업로드·조회·다운로드·삭제하는 FastAPI 서버입니다. 인터넷이 없는 폐쇄망에서도 Swagger UI가 동작합니다. UE 쪽 클라이언트는 `Source/DWO_Proj_01/AttributeSample/AssetServerSubsystem`입니다.

## 실행

```
run_server.bat          (포트 8000)
run_server.bat 9000     (포트 지정)
```

처음 실행하면 `.venv`를 만들고 `requirements.txt`의 패키지를 설치합니다. 이후 브라우저에서 `http://localhost:8000/docs`를 엽니다.

직접 실행할 때는 다음과 같이 합니다.

```
python -m venv .venv
.venv\Scripts\python -m pip install -r requirements.txt
.venv\Scripts\python -m uvicorn main:app --host 0.0.0.0 --port 8000
```

다른 PC에서 접속하려면 Windows 방화벽에서 해당 포트를 허용해야 합니다.

## API

| 메서드 | 경로 | 요청 | 응답 |
|---|---|---|---|
| POST | `/assets` | multipart/form-data (`file`) | JSON (Swagger, curl용) |
| POST | `/assets/raw?name=Gun.glb` | 본문 = 파일 바이트 (`application/octet-stream`), 헤더 `X-Content-SHA256` 선택 | JSON (UE HttpModule 권장) |
| PUT | `/assets/{id}/raw?name=Gun.glb` | 위와 같음. **id를 유지한 채 파일 교체** | JSON |
| POST | `/assets/json` | `{"name": "...", "data_base64": "..."}` | JSON (20MB 이하만) |
| GET | `/assets?q=이름&limit=&offset=` | | JSON 배열 |
| GET | `/assets/{id}?verify=true` | | JSON (name, ext, size, sha256, updated_at, download_url). `verify=true`면 저장된 파일을 다시 해시해 `stored_sha256`, `intact`, `file_present` 추가 |
| GET | `/assets/{id}/download` | | 바이너리 |
| GET | `/assets/{id}/base64` | | JSON + `data_base64` (20MB 이하만) |
| DELETE | `/assets/{id}` | | JSON |

- 업로드 제한: `.glb` / `.gltf` / `.fbx`만 허용하며 최대 500MB입니다. GLB는 헤더와 JSON 청크를 검사하고 메시·머티리얼·텍스처 개수를 `summary`에 기록합니다. FBX는 파일 시그니처만 확인합니다.
- 무결성:
  - 업로드에 `X-Content-SHA256`(본문의 sha256, 16진수 64자)을 보내면, 서버가 받은 파일의 해시와 비교해 다르면 **422**로 거부합니다. 이 경우 저장소에는 아무것도 남지 않습니다.
  - 업로드는 임시 파일에 먼저 받은 뒤 원자적으로 교체하므로, 다운로드하는 쪽이 반쯤 쓰인 파일을 보는 일이 없습니다.
  - `verify=true`는 저장된 파일이 기록된 해시와 같은지(`intact`) 알려 줍니다.
- 저장 위치: 파일은 `storage/<id>.<ext>`, 메타데이터는 `assets.db`(SQLite)에 저장됩니다. 둘 다 git에 넣지 않습니다. v0.1에서 만든 DB는 처음 실행할 때 `updated_at` 컬럼이 자동으로 추가됩니다.

```
curl -X POST "http://localhost:8000/assets/raw?name=Gun.glb" -H "Content-Type: application/octet-stream" -H "X-Content-SHA256: <sha256>" --data-binary "@Gun.glb"
curl "http://localhost:8000/assets/<id>?verify=true"
curl -o Gun.glb http://localhost:8000/assets/<id>/download
```

## UE에서 쓸 때

- 메타데이터는 JSON으로 주고받고, 파일 바이트는 바이너리 그대로 보냅니다. base64를 쓰면 크기가 33% 늘고 파일 전체가 메모리에 올라갑니다.
- 다운로드 응답은 `GetContent()`(TArray<uint8>)로 받아야 합니다. `GetContentAsString()`을 쓰면 바이너리가 깨집니다.
- 사용자 에셋 동기화는 `UUserAssetLibrary`가 담당합니다(`Tools/AttributeSample/README.md`의 "사용자 에셋 저장과 서버 동기화" 참고). 서버 주소는 기본값이 `http://localhost:8000`이고, 실행 인자 `-AssetServerUrl=http://호스트:포트`로 바꿀 수 있습니다.
- 블루프린트의 `Download Asset`은 `Saved/AssetCache/<sha256><ext>`에 캐시하고, 해시가 같은 파일이 있으면 다시 받지 않습니다.

## 폐쇄망 반입

1. 인터넷 PC에서 `pip download -r requirements.txt -d wheels`를 실행합니다.
2. 이 폴더와 `wheels` 폴더를 함께 옮깁니다. `run_server.bat`이 `wheels`를 찾아 오프라인으로 설치합니다.
3. Swagger UI 파일은 `static/swagger`에 들어 있어서 CDN이 필요 없습니다(swagger-ui-dist 5.17.14, Apache-2.0, `static/swagger/LICENSE` 참고).

## 아직 없는 것

- 인증과 권한: 지금은 누구나 업로드하고 삭제할 수 있습니다.
- NAS / MinIO 저장소: `save_asset()`과 다운로드 경로를 S3 클라이언트로 바꾸면 됩니다.
