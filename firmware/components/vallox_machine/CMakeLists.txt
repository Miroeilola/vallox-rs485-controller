idf_component_register(SRCS "vallox_machine.c" "vallox_machine_physics.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "."
                       REQUIRES vallox_protocol)
