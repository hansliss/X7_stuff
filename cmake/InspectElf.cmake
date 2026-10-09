execute_process(COMMAND "${X7_READELF}" -h -l -S "${X7_ELF}"
    OUTPUT_FILE "${X7_REPORT}.elfstruct" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${X7_OBJDUMP}" -d "${X7_ELF}"
    OUTPUT_FILE "${X7_REPORT}.dis" COMMAND_ERROR_IS_FATAL ANY)
