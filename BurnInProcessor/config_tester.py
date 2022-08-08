import os, json
import jsonschema
from jsonschema import validate


def get_config_path():
    input_path = input("Enter path to config folder / Leave blank to scan current folder:\n")
    input_path = input_path.replace("\\", "/")

    if input_path == "":
        return os.getcwd()

    while os.path.isdir(input_path) == False:
        print()
        print("Unable to find a directory called \"{}\", please check for spelling mistakes.".format(input_path))
        input_path = input("Enter the path to the config folder / Leave empty to scan this folder:\n")
    
    return input_path

def get_schema_path():
    input_path = input("Enter path to ")

def check_valid_json(fp):
    try:
        json.load(fp)
    except ValueError as err:
        return False
    return True

def get_json(folder_path):
    configs = []
    for path in os.listdir(folder_path):
        print(path)
        with open(folder_path + "/" + path, "r") as fp:
            if check_valid_json(fp):
                configs.append(json.loads(fp.read()))
    return configs

def check_valid_schema(json_data, valid_schema):
    try:
        validate(instance=json_data, schema=valid_schema)
    except jsonschema.exceptions.ValidationError as err:
        return False
    return True

js = get_json(get_config_path())

print("\n\njson configs:\n\n")

for j in js:
    print(j)
