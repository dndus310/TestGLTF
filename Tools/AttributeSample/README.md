# Attribute Sample

UE 5.7 샘플: DataTable 기반 Attribute 목록 → 뷰포트 스폰, 그리고 사용자 메시 파일(glTF/GLB/FBX)을 런타임에 불러와 Attribute에 적용합니다.

## 실행

1. `DWO_Proj_01.uproject`를 열고 `/Game/AttributeSample/Maps/Lvl_AttributeSample`을 엽니다.
2. Play를 누릅니다. (World Settings의 GameMode Override = `AttributeSampleGameMode`)

| 조작 | 동작 |
|---|---|
| LeftPanel 항목 클릭 | 해당 Attribute를 임의 좌표에 스폰 |
| Building / Load 탭 | 레이어별로 목록 필터 |
| `+ Add Attribute` | RightPanel(Asset Library) 열기 |
| 타일 선택 → `Apply` | 선택 에셋을 LeftPanel 목록에 추가 |
| `Import User Asset...` | 탐색기 대화상자 → glTFRuntime(FBX는 glTFRuntimeFBX)으로 로드 → 카탈로그에 추가 |
| 우클릭 드래그 / 휠 | 카메라 회전 / 줌 |

콘솔 명령 (개발 빌드): `AttributeSpawnAll`, `AttributeShowLibrary 0|1`, `AttributeImportFile <경로> 0|1`

## 구성

| 사양 | 구현 |
|---|---|
| `AttributeAssetData` | `FAttributeAssetData` (Name, AssetType, SoftObjectStaticMeshAsset, ThumbnailPath) |
| `DT_AttributeAssetData` | `/Game/AttributeSample/Data/` (10행: Building 5, Load 5) |
| `WBP_LeftPanel` | `UAttributeLeftPanelWidget` : Layer(HorizontalBox) + `WBP_AttributeList` |
| `WBP_AttributeList` | `UAttributeListWidget` : List(VerticalBox) + AddAttribute(Button) |
| `WBP_RightPanel` | `UAttributeRightPanelWidget` : Layer + `WBP_AttributeAssetList` |
| `WBP_AttributeAssetList` | `UAttributeAssetListWidget` : List(TileView, Columns x VisibleRows) + Apply |
| `WBP_AttributeAssetItem` | `UAttributeAssetItemWidget` (TileView 엔트리) |
| `CoreAttribute` | `ACoreAttribute` : SplineComponent + StaticMeshComponent(DefaultMesh), `ApplyRuntimeSetMeshAsset`, `MakeSplinePointsByShapeBounding` |
| `BuildingAttribute`, `LoadAttribute` | `ABuildingAttribute`, `ALoadAttribute` |

코드: `Source/DWO_Proj_01/AttributeSample/`

- 위젯 레이아웃은 WBP 디자이너 탭에 들어 있습니다. 런타임에는 같은 이름의 위젯(`Layer`, `BuildingTab`, `LoadTab`, `List`, `AddAttribute`, `Apply`, `ImportUserAsset` 등)이 `BindWidgetOptional`로 연결됩니다. 디자이너 트리가 비어 있을 때만 C++ 기본 레이아웃(`BuildDefaultLayout`)이 대신 쓰입니다.

## C++ 레이아웃 → 디자이너 변환

C++ 기본 레이아웃을 5개 WBP의 디자이너 트리로 써 넣는 에디터 도구입니다(`Source/DWO_Proj_01Editor`, 에디터 전용 모듈이라 패키지에는 들어가지 않음). 바인딩되는 위젯만 Is Variable로 설정하고, 컴파일과 저장까지 합니다.

- 에디터 메뉴: **Tools > Attribute Sample > Export Widget Layouts to Designer**
  - 기본: 디자이너 트리가 비어 있는 WBP만 씁니다.
  - `(Overwrite)`: 디자이너에서 수정한 내용을 지우고 C++ 레이아웃으로 다시 씁니다(확인 창이 뜹니다).
- 명령줄:

```
UnrealEditor-Cmd.exe DWO_Proj_01.uproject -run=AttributeWidgetDesignerExport [-Overwrite]
```

- Python: `unreal.AttributeWidgetDesignerLibrary.export_attribute_widgets_to_designer(False)`
- `ThumbnailPath`가 비어 있으면 썸네일을 런타임에 렌더링합니다(`AAttributeThumbnailStudio`). 텍스처 경로(`/Game/...`)나 이미지 파일 경로를 넣어도 됩니다.
- 사용자 파일 제한: 256MB 이하, 삼각형 300만 개 이하, `.gltf/.glb/.fbx`만 허용 (`UAttributeRuntimeMeshLoader`).

## 에셋 재생성

DataTable / WBP / 레벨은 `create_attribute_sample_assets.py`가 만듭니다. 다시 실행해도 안전합니다(DT 행은 덮어씁니다).

```
UnrealEditor-Cmd.exe DWO_Proj_01.uproject -run=pythonscript -script="Tools/AttributeSample/create_attribute_sample_assets.py"
```

## 패키징

- `DefaultGame.ini`에 `/Game/AttributeSample`, `/glTFRuntime`, `/glTFRuntimeFBX`를 항상 쿠킹하도록 추가했습니다. (DT와 WBP는 소프트 경로로만 참조됩니다.)
- `Licenses/ThirdPartyNotices.txt`가 패키지 폴더에 함께 복사됩니다(NonUFS).
