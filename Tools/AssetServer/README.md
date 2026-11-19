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
| POST | `/assets/raw?name=Gun.glb` | 본문 = 파일 바이트 (`application/octet-stream`) | JSON (UE HttpModule 권장) |
| POST | `/assets/json` | `{"name": "...", "data_base64": "..."}` | JSON (20MB 이하만) |
| GET | `/assets?q=이름&limit=&offset=` | | JSON 배열 |
| GET | `/assets/{id}` | | JSON (name, size, sha256, summary, download_url) |
| GET | `/assets/{id}/download` | | 바이너리 (GLB) |
| GET | `/assets/{id}/base64` | | JSON + `data_base64` (20MB 이하만) |
| DELETE | `/assets/{id}` | | JSON |

- 업로드 제한: `.glb` / `.gltf`만 허용하며 최대 500MB입니다. GLB는 헤더와 JSON 청크를 검사하고, 메시·머티리얼·텍스처 개수를 `summary`에 기록합니다.
- 저장 위치: 파일은 `storage/<id>.<ext>`, 메타데이터는 `assets.db`(SQLite)에 저장됩니다. 둘 다 git에 넣지 않습니다.

```
curl -X POST "http://localhost:8000/assets/raw?name=Gun.glb" -H "Content-Type: application/octet-stream" --data-binary "@Gun.glb"
curl -o Gun.glb http://localhost:8000/assets/<id>/download
```

## UE에서 쓸 때

- 메타데이터는 JSON으로 주고받고, 파일 바이트는 바이너리 그대로 보냅니다. base64를 쓰면 크기가 33% 늘고 파일 전체가 메모리에 올라갑니다.
- 다운로드 응답은 `GetContent()`(TArray<uint8>)로 받아야 합니다. `GetContentAsString()`을 쓰면 바이너리가 깨집니다.
- `AssetServerSubsystem`은 `Saved/AssetCache/<sha256>.glb`에 캐시하고, 같은 해시가 있으면 다시 받지 않습니다.

## 폐쇄망 반입

1. 인터넷 PC에서 `pip download -r requirements.txt -d wheels`를 실행합니다.
2. 이 폴더와 `wheels` 폴더를 함께 옮깁니다. `run_server.bat`이 `wheels`를 찾아 오프라인으로 설치합니다.
3. Swagger UI 파일은 `static/swagger`에 들어 있어서 CDN이 필요 없습니다(swagger-ui-dist 5.17.14, Apache-2.0, `static/swagger/LICENSE` 참고).

## 아직 없는 것

- 인증과 권한: 지금은 누구나 업로드하고 삭제할 수 있습니다.
- NAS / MinIO 저장소: `save_asset()`과 다운로드 경로를 S3 클라이언트로 바꾸면 됩니다.
