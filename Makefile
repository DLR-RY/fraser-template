hosts_config_file?=config0.xml
model?=
ANSIBLE_DIR := ansible

all:
	make configure-local
	make update
	make configure
	make init
	make build-all
	make default-configs
#	make deploy
#	make run-all

help:
	@echo "Please use \`make <target>\` where <target> is one of"
	@echo "  configure-local						to configure the localhost (install dependencies)"
	@echo "  update									to update inventory file (hosts definition)"
	@echo "  init                                   to dissolve model dependencies and generate C++ header files from the flatbuffers"
	@echo "  configure                              to configure the hosts (install dependencies)"
	@echo "  build-all                              to build the models"
	@echo "  build model=<name>                     to build a specific model"
	@echo "  default-configs                        to create default configuration files (saved in \`configurations/config_0\`)"
	@echo "  deploy                                 to deploy the software to the hosts"
	@echo "  run-all                                to run models on the hosts"
	@echo "  run model=<name>                       to run a specific custom model"
	@echo "  clean                                  to remove temporary data (\`build\` folder)"

configure-local:
	ansible-playbook $(ANSIBLE_DIR)/configure-local.yml --ask-become-pass --connection=local -e ansible_python_interpreter=/usr/bin/python -i ./ansible/inventory/hosts

update:
	ansible-playbook $(ANSIBLE_DIR)/update-inv.yml --connection=local -e hosts_config_file=$(hosts_config_file)

configure:
	ansible-playbook $(ANSIBLE_DIR)/configure.yml --ask-become-pass -i ./ansible/inventory/hosts

init:
	ansible-playbook $(ANSIBLE_DIR)/init.yml --connection=local -i ./ansible/inventory/hosts

build-all:
	ansible-playbook $(ANSIBLE_DIR)/build.yml --connection=local -i ./ansible/inventory/hosts

build:
	ansible-playbook $(ANSIBLE_DIR)/build.yml --connection=local -i ./ansible/inventory/hosts -e 'models=[{"name":"$(model)"}]'

default-configs:
	ansible-playbook $(ANSIBLE_DIR)/default-configs.yml --connection=local -i ./ansible/inventory/hosts

deploy:
	ansible-playbook $(ANSIBLE_DIR)/deploy.yml -i ./ansible/inventory/hosts

run-all:
	ansible-playbook $(ANSIBLE_DIR)/run.yml -i ./ansible/inventory/hosts

run:
	models/$(model)/build/bin/$(model)

list-models-info:
	cat ansible/inventory/group_vars/all/main.yml

clean:
	ansible-playbook $(ANSIBLE_DIR)/clean.yml -i ./ansible/inventory/hosts
