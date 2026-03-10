# JSN-SR04T Renode Plugin

C# plugin implementation of the JSN-SR04T ultrasonic sensor model.

## Build

```bash
cd simulation/src/JSN_SR04T_Plugin
./build.sh /path/to/renode/bin-or-output-dir
```

The script resolves Renode binaries in this order:
1. first argument to `build.sh`
2. `RENODE_BIN_DIR` env var
3. `../../../../renode/output/bin/Release`
4. `~/.net/renode/ThLhnt0ejjXPsWUsqImYupJxhG6fITM=`

Output:

- `bin/Release/net8.0/JSN_SR04T_Plugin.dll`

## Notes

For the current simulation flow, the Python helper (`simulation/python/sensor_helper.py`)
is the default runtime path. The plugin remains available when C# peripheral integration
is preferred.
