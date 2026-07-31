Import("env")

from pathlib import Path

project_dir = Path(env.subst("$PROJECT_DIR"))
build_dir = Path(env.subst("$BUILD_DIR"))
generated_dir = build_dir / "safety_static"
generated_dir.mkdir(parents=True, exist_ok=True)
def write_source_header(source_name, header_name, symbol):
    source = (project_dir / "src" / source_name).read_text()
    escaped = source.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n"\n"')
    (generated_dir / header_name).write_text("#pragma once\nstatic const char " + symbol + "[] =\n\"" + escaped + "\";\n")


write_source_header("flight_control.cpp", "flight_control_source.hpp", "FLIGHT_CONTROL_SOURCE")
write_source_header("telemetry.cpp", "telemetry_source.hpp", "TELEMETRY_SOURCE")
env.Append(CPPPATH=[str(generated_dir)])
