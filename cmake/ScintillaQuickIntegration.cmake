# Runs in ScintillaQuick's project scope. Metatype custom commands must belong
# to the dependency's directory so CMake attaches them to its build graph.
cmake_language(DEFER CALL qt_extract_metatypes ScintillaQuick)
