#!/bin/bash

models/configuration_server/build/bin/configuration_server --config-file hosts-configs/config1.xml &
models/logger/build/bin/logger --log-files-path logs/ &

models/model_1/build/bin/model_1 -n model_1 &
models/model_2/build/bin/model_2 -n model_2 &
models/event_queue_1/build/bin/event_queue_1 -n event_queue_1 &

models/simulation_model/build/bin/simulation_model --load-config configurations/config_0/
