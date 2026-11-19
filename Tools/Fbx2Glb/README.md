# Fbx2Glb

설치된 UE 5.7 에디터만으로 FBX를 GLB로 변환합니다. Interchange로 FBX를 씬(레벨)으로 import한 뒤 내장 GLTFExporter로 월드를 export합니다. 파츠별 메시와 노드 계층이 그대로 유지됩니다.

## 변환

```
convert.bat Gun.fbx out\Gun.glb Gun_T.png Gun_N.png
```

- 3번째·4번째 인자(BaseColor / Normal 텍스처)는 생략할 수 있습니다. 텍스처를 넣으면 모든 메시에 한 머티리얼(BaseColor, Normal)을 만들어 지정합니다.
- 엔진 경로가 다르면 `UE_ROOT` 환경변수로 지정합니다(기본값: `C:\Program Files\Epic Games\UE_5.7`).
- 변환은 `ConvertProj`라는 임시 프로젝트에서 실행되며, 원래 게임 프로젝트는 건드리지 않습니다.

## 확인

```
check.bat out\Gun.glb out\check
python -I inspect_glb.py out\Gun.glb
```

- `check.bat`: GLB를 Interchange로 다시 불러와 들어온 에셋을 로그로 남기고, `check_basecolor.png`·`check_lit.png`·`check_lit_2.png`를 렌더링합니다.
- `inspect_glb.py`: UE 없이 파이썬만으로 GLB 헤더·청크·bufferView 범위를 검사하고, 메시·머티리얼·이미지와 노드 변환까지 반영한 전체 크기를 출력합니다.

## 주의

- **GPU가 필요합니다.** 머티리얼 베이크가 렌더링을 쓰기 때문에 `-nullrhi`로는 텍스처가 빠집니다. GPU가 없는 헤드리스 리눅스 서버에는 맞지 않습니다. 서버에서 대량으로 변환하려면 Blender 헤드리스나 FBX2glTF가 더 적합합니다.
- **실행 시간:** 실행할 때마다 에디터를 띄워서 20~60초가 걸립니다.
- **콘텐츠 브라우저 크래시:** 커맨드릿에는 UI가 없어서 import 뒤 콘텐츠 브라우저 동기화가 크래시를 냅니다. 스크립트에서 `Interchange.FeatureFlags.Import.SyncToBrowser 0`으로 막아 두었습니다.
- **스케일:** 원본 노드 스케일을 그대로 옮깁니다. Blender에서 export한 FBX는 100배 스케일이 걸려 있는 경우가 많으니 결과 크기를 확인하세요.
- **에디터 필요:** Interchange FBX 파서는 에디터 전용 모듈이라, 이 도구는 UE 에디터가 설치된 PC에서만 동작합니다(패키징된 게임에서는 쓸 수 없음).
