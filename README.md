
# 3D Tiles for Godot

A Godot 4 extension that integrates Cesium 3D Tiles capabilities into Godot Engine through GDExtension.

## 3D Tiles and the 3D Geospatial Ecosystem
3D Tiles for Godot streams real-world 3D content such as high-resolution photogrammetry, terrain, imagery, and 3D buildings from Cesium Ion and other sources available as optional commercial subscriptions. The plugin includes Cesium Ion integration for instant access to global high-resolution 3D content ready for runtime streaming. Cesium ion users can also leverage cloud-based 3D tiling pipelines to create end-to-end workflows to transform massive heterogenous content into semantically-rich 3D Tiles, ready for streaming to the Godot Engine.

3D Tiles for Godot supports cloud and private network content and services based on open standards and APIs. You are free to use any combination of supported content sources, standards, APIs with 3D Tiles for Godot.

## Godot Engine Integration
3D Tiles for Godot is tightly integrated with the Godot Engine making it possible to visualize and interact with real-world content in editor and at runtime. The plugin also has support for Godot physics, collisions, and character interaction. 

Leverage decades worth of cutting-edge advancements from the 3D Tiles standard to create cohesive, interactive, and realistic simulations and applications.

## Supported Platforms
As of now the plugin is available for Windows, Linux, and **macOS (Apple Silicon)** as development platforms, with support for Web and Android coming in the future. See [`docs/BUILD_MACOS.md`](docs/BUILD_MACOS.md) for macOS build instructions.

![Godot Cesium Plugin](readme_resources/NYC_Screenshot.png)

## Prerequisites

- **C++ 20**
- **CMake 3.1+ and less than 4.0 due to compatibility issues**
- **Python 3.8+** (with pip or uv)
- **SCons** (`pip install scons` or `uvx scons`)
- **Git**
- **Godot 4.1+** (with GDExtension support)
- **Linux specific**
  - **libcurl4-openssl-dev**
  - **x86 architecture**
- **Windows specific:**
  - **Visual Studio 2022** with C++ desktop development workload (If you plan on contributing or developing)
  - **Windows 10/11** (64-bit)

## Installation (as GDExtension)

### 1. Clone the Repository
```bash
git clone https://github.com/Battle-Road/3D-Tiles-For-Godot.git
cd 3D-Tiles-For-Godot
```

If you would like to build the binaries and you don't plan on modifying the underlying C++ code with any editor use the following command
```
scons arch=x64 compileTarget=extension target=template_release production=yes
```

Otherwise, to get a `compile_commands.json` file to use with an IDE or Text Editor, run
```
scons compiledb=yes arch=x64 compileTarget=extension debug_symbols=yes
```

#### Build Flags Explained:

* `compiledb=yes`: Generate IDE-friendly compilation database (optional)

* `arch=x64`: 64-bit architecture, will be used as build architecture for all subsequent libraries 

* `compileTarget=extension`: Build as GDExtension

* `debug_symbols=yes`: Include debug information (use no for release)

**The build binaries will be at `godot3dtiles/bin/Godot3DTiles.windows.template_debug.x86_64.dll` for Windows, `godot3dtiles/bin/libGodot3DTiles.linux.template_debug.x86_64.so`**

### Linux release builds

A Linux library only loads on systems whose glibc and libstdc++ are at least as new as those of the machine that built it. Releases are therefore built on Debian bookworm, which is the base of havoc-canvas's runtime image. Pushing a `v*` tag runs `.github/workflows/linux-release.yml`. The workflow builds the addon with `scripts/release/build_linux.sh`, loads it in headless Godot with `scripts/release/smoke_test.sh`, and attaches these files to the tag's release:

- `cesium_godot-linux-x86_64.tar.gz`: the `cesium_godot/` addon with the stripped library. Unpack it into a project's `addons/` directory.
- `cesium_godot-linux-x86_64-debug.tar.gz`: the library's debug info, for reading crash backtraces.
- `SHA256SUMS`: the checksums of both archives.

To build the same archives locally, run the script in the same image as CI. The command for this is at the top of `scripts/release/build_linux.sh`.


## Double precision builds
The plugin supports double precision mode for more accurate true origin navigation.

### Steps to build with doubles

* Add `precision=double` to the build flags on the plugin
* Download [Godot's source code on github](https://github.com/godotengine/godot)
* Build Godot with `scons platform=windows arch=x64 precision=double`
* Modify the `Godot3DTiles.gdextension` file to point to your double precision dll


## Setting Up a Project
### Create New Godot Project

* Use Godot 4.1 or newer

* Choose "Forward+" renderer in project settings

### Copy Plugin Files

* From the repository:

  * Copy `godot3dtiles/addons/` folder to your project root

Your project structure should look like:

```
my_project/
├── addons/
│   └── cesium_godot/
│       ├── lib/
│       ├── panels/
│       ├── resources/
│       ├── scripts/
│       ├── shaders/
│       ├── visuals/
│       ├── plugin.cfg
│       └── Godot3DTiles.gdextension
└── project.godot
```

Alternatively, you can choose to create a new project using the `godot3dtiles/` folder as the project's root

### Enable the plugin

* Open your project in Godot

* Go to Project → Project Settings → Plugins

* Enable "cesium_godot"

## Getting Started

### Using the Cesium Ion Panel

![PanelDemo1](readme_resources/panel_default.png)

The Cesium Ion Panel provides essential tools for managing geospatial content and configuring plugin integration with Cesium services.

**Connect to Cesium ION**


* Select Connect to Cesium ION to connect the plugin to your Cesium account.

* Requires valid Cesium ION credentials

* Grants access to your private assets and token-protected resources

* Maintains session security through OAuth2 authorization flow.

Once you've connected and authorized the application, you should see your Cesium Assets populate in the docked panel.

![PanelDemo2](readme_resources/panel_populated.png)

### Setting up your first scene

![CreateScene](readme_resources/create_scene.png)

**Create a new 3D scene**

**Add a new tileset**
You can choose between the different pre-defined tilesets provided to you on the Cesium Ion Panel, or create a new blank tileset to customize it with your desired asset id and data source URL.

You should now see multiple nodes were added to your scene, depending on the option you picked.

*Example with pre-defined tileset Cesium World Terrain + Bing Maps Aerial Imagery*
![AddTileset](readme_resources/add_tileset.png)

**Add a dynamic camera**

Back to the Cesium Panel, you should now be able to click on the dynamic camera button and see it get added to your scene.

![DynamicCamera](readme_resources/dynamic_camera.png)

You should also see the tilesets and CesiumGeoreference have been added automatically to the camera's properties in the inspector.

![CameraProperties](readme_resources/camera_properties.png)

**Hit the Play button**
Tiles should start loading in now!

## Scripting

### Selecting tiles

`Cesium3DTileset.update_tileset(camera_transform)` selects and loads tiles for one camera. `camera_transform` is in ECEF axes, e.g. `georeference.get_tx_engine_to_ecef() * camera.global_transform`. With a cartographic origin, its origin is the camera's offset from the georeference's point. Field of view and viewport size come from the viewport's camera, or defaults when there is none.

`Cesium3DTileset.update_tileset_views(views)` selects tiles for any number of views and renders their union, with no camera needed. This is useful for loading tiles around vehicles for collision as well as for each render camera. Each view is a Dictionary with:

* either `"transform"` (`Transform3D`, same frame as `update_tileset`, looking along -Z), or `"position"` (`Vector3`, same frame) / `"ecef_position"` (`[x, y, z]` absolute ECEF in double precision) plus `"direction"` (ECEF axes) and an optional `"up"`
* optional `"fov"` (vertical, degrees, default 75) and `"size"` (viewport pixels used for screen-space error, default 1920×1080)

```gdscript
tileset.update_tileset_views([
    {"transform": georeference.get_tx_engine_to_ecef() * camera.global_transform, "fov": camera.fov, "size": Vector2(get_viewport().size)},
    {"ecef_position": vehicle_ecef, "direction": -vehicle_up_ecef, "fov": 120.0, "size": Vector2(512, 512)},
])
```

### Feature metadata

Tiles (`Cesium3DTile`, the tileset's children) expose `EXT_structural_metadata` and `EXT_mesh_features`, including batch tables that cesium-native converts. For example, the properties of an OSM building hit by a ray against the tile's collision shape:

```gdscript
var hit := space_state.intersect_ray(query)
var tile = hit.collider.get_parent() # Cesium3DTile
var properties: Dictionary = tile.get_face_feature_properties(hit.face_index)
```

* `get_feature_count(table = 0)` and `get_feature_properties(feature_id, table = 0)` return one feature's properties
* `get_feature_id_set_count()`, `get_vertex_feature_ids(surface, set = 0)` and `get_face_feature_id(face_index, set = 0)` map vertices and faces (as in `Mesh.get_faces()`) to feature IDs
* `get_metadata_table(table)` returns `{name: CesiumPropertyInfo}`, where `get_value()` holds one value per feature and `get_value_at(feature_id)` returns one

### Data attribution

Credits are shown in an overlay at the bottom left. `CesiumGDCreditSystem.set_credits_collapsed(true)` (or the overlay's `collapsed` property) keeps only the credits that must stay on screen and adds a "Data attribution" link that lists the rest.

