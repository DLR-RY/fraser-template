#!/bin/bash

models/configuration_server/build/bin/configuration_server --config-file hosts-configs/config1.xml &
models/logger/build/bin/logger --log-files-path logs/ &

models/temp_sensor_reader/build/bin/temp_sensor_reader -n temp_sensor_reader_1 &
models/temp_sensor_reader/build/bin/temp_sensor_reader -n temp_sensor_reader_2 &
models/thermal_controller/build/bin/thermal_controller -n thermal_controller &
models/event_queue_1/build/bin/event_queue_1 -n event_queue_1 &

models/simulation_model/build/bin/simulation_model --load-config configurations/config_0/
