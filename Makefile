CONFIGURATION_SERVER_DIR := models/configuration_server
SIMULATION_MODEL_DIR := models/simulation_model
EVENT_QUEUE_DIR := models/event_queue_1
MODEL_1_DIR := models/model_1
MODEL_2_DIR := models/model_2
ANSIBLE_DIR := ansible

help:
	@echo "Please use \`make <target>\` where <target> is one of"
	@echo "  configure          to configure the hosts"
	@echo "  init               to initial the template and dissolve dependencies"
	@echo "  build              to build the models"
	@echo "  default-configs    to create default configuration files (saved in \`configurations/config_0\`)"
	@echo "  deploy             to run all models"
	@echo "  clean              to remove temporary data (\`build\` folder)"

configure:
	ansible-playbook $(ANSIBLE_DIR)/configure.yml --ask-become-pass --inventory ansible/inventory/hosts

init:
	ansible-playbook $(ANSIBLE_DIR)/init.yml --connection=local --inventory ansible/inventory/hosts

build:
	@$(MAKE) -C $(CONFIGURATION_SERVER_DIR) -f Makefile
	@$(MAKE) -C $(SIMULATION_MODEL_DIR) -f Makefile
	@$(MAKE) -C $(EVENT_QUEUE_DIR) -f Makefile
	@$(MAKE) -C $(MODEL_1_DIR) -f Makefile
	@$(MAKE) -C $(MODEL_2_DIR) -f Makefile

default-configs:
	ansible-playbook $(ANSIBLE_DIR)/default-configs.yml --connection=local --inventory ansible/inventory/hosts

deploy:
	ansible-playbook $(ANSIBLE_DIR)/deploy.yml --inventory ansible/inventory/hosts

clean:
	@$(MAKE) -C $(CONFIGURATION_SERVER_DIR) -f Makefile clean
	@$(MAKE) -C $(SIMULATION_MODEL_DIR) -f Makefile clean
	@$(MAKE) -C $(EVENT_QUEUE_DIR) -f Makefile clean
	@$(MAKE) -C $(MODEL_1_DIR) -f Makefile clean
	@$(MAKE) -C $(MODEL_2_DIR) -f Makefile clean