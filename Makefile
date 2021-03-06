hosts_config_file?=config1.xml
remote_home_path?=~/tmp_simulation
ANSIBLE_DIR := ansible

all:
	make prepare-localhost
	make update
	make configure
	make initialize
	make build

help:
	@echo " [LOCAL] Please use \`make <target>\` where <target> is one of"
	@echo "  configure                              to prepare the localhost (install dependencies)"
	@echo "  update                                 to update inventory file (hosts definition) for ANSIBLE and debug scripts"
	@echo "  configure                              to configure the hosts (install required dependencies for all hosts (also localhost)"
	@echo "  initialize                             to dissolve model dependencies and generate C++ header files from the flatbuffers"
	@echo "  build                                  to build the models"
	@echo "  create-default-configs                 to create default configuration files (saved in \`configurations/config_0\`)"
	@echo "  run-local                              to run models on localhost"
	@echo ""
	@echo " [DEBUG] Please use \`make <target>\` where <target> is one of"
	@echo "  debug-create-default-configs           to run bash script to create default configuration files (saved in \`configurations/config_0\`)"
	@echo "  debug-run-local                        to run bash script to run models on localhost"
	@echo ""
	@echo " [REMOTE] Please use \`make <target>\` where <target> is one of"
	@echo "  deploy                                 to deploy the software to the hosts (\`tmp_simulation\` folder)"
	@echo "  run-remote                             to run models on the hosts (remotely or locally)"
	@echo ""
	@echo "  clean                                  to remove temporary data (\`build\` folder)"

prepare-localhost:
	ansible-playbook $(ANSIBLE_DIR)/configure-local.yml --ask-become-pass --connection=local -e ansible_python_interpreter=/usr/bin/python -i ./ansible/inventory/hosts

update:
	ansible-playbook $(ANSIBLE_DIR)/update-inv.yml --connection=local -e hosts_config_file=$(hosts_config_file)

configure:
	ansible-playbook $(ANSIBLE_DIR)/configure.yml --ask-become-pass -i ./ansible/inventory/hosts

initialize:
	ansible-playbook $(ANSIBLE_DIR)/init.yml --connection=local -i ./ansible/inventory/hosts

build:
	ansible-playbook $(ANSIBLE_DIR)/build.yml --connection=local -i ./ansible/inventory/hosts

create-default-configs :
	ansible-playbook $(ANSIBLE_DIR)/default-configs.yml --connection=local -i ./ansible/inventory/hosts

run-local:
	ansible-playbook $(ANSIBLE_DIR)/run-local.yml --connection=local -i ./ansible/inventory/hosts

debug-create-default-configs:
	sh debug-scripts/create_default_configurations.sh

debug-run-local:
	sh debug-scripts/start_simulation.sh

deploy:
	ansible-playbook $(ANSIBLE_DIR)/deploy.yml -i ./ansible/inventory/hosts -e remote_home_path=$(remote_home_path)

run-remote:
	ansible-playbook $(ANSIBLE_DIR)/run-remote.yml -i ./ansible/inventory/hosts -e remote_home_path=$(remote_home_path)

list-models-info:
	cat ansible/inventory/group_vars/all/main.yml

clean:
	ansible-playbook $(ANSIBLE_DIR)/clean.yml -i ./ansible/inventory/hosts
