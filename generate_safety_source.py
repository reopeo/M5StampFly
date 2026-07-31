Import("env")

from pathlib import Path

project_dir = Path(env.subst("$PROJECT_DIR"))
build_dir = Path(env.subst("$BUILD_DIR"))
generated_dir = build_dir / "safety_static"
generated_dir.mkdir(parents=True, exist_ok=True)
source = (project_dir / "src" / "flight_control.cpp").read_text()
escaped = source.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n"\n"')
(generated_dir / "flight_control_source.hpp").write_text(
    "#pragma once\nstatic const char FLIGHT_CONTROL_SOURCE[] =\n\"" + escaped + "\";\n"
)
env.Append(CPPPATH=[str(generated_dir)])
