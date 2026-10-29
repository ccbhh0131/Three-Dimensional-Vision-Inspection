# Vision3D Inspector application

The current application combines a Qt Quick / QML shell with the Qt Widgets / OpenGL viewer, photo/video reconstruction and reviewed equipment inspection.

- [中文项目说明、当前界面和构建方法](../README.md)
- [English overview and build instructions](../README_EN.md)
- [Experimental local worker](local_ai/README.md)

Configure from the repository root using `cmake -S app -B build` and the dependency options documented above. Keep local dependency paths in an untracked `CMakeUserPresets.json`; external runtimes and private datasets are not included.
