#!/usr/bin/env python

import os
import re
import argparse
from enum import Enum


THIS_FILE_DIR = os.path.dirname(os.path.abspath(__file__))
DEMO_CONFIG_FILE_PATH = os.path.abspath(os.path.join(THIS_FILE_DIR, '..', '..', '..', '..', 'source', 'configuration-files', 'demo_config.h'))
GENERATED_DEMO_CONFIG_FILE_PATH = os.path.join(THIS_FILE_DIR, 'demo_config.h')


class CredentialType(Enum):
    PRIV_KEY = 1
    CERT = 2
    ROOT_CA = 3
    THING_NAME = 4
    ENDPOINT = 5


CREDS_IDENTIFIER_MAP = {
    CredentialType.PRIV_KEY: '#define democonfigCLIENT_PRIVATE_KEY_PEM',
    CredentialType.CERT: '#define democonfigCLIENT_CERTIFICATE_PEM',
    CredentialType.ROOT_CA: '#define democonfigROOT_CA_PEM',
    CredentialType.THING_NAME: '#define democonfigCLIENT_IDENTIFIER',
    CredentialType.ENDPOINT: '#define democonfigMQTT_BROKER_ENDPOINT'
}


def prepare_cred_string(cred_file):
    with open(cred_file, 'r') as f:
        cred_file_content = f.read()

    cred_lines = cred_file_content.rstrip('\n').split('\n')
    cred_string = ''
    for cred_line in cred_lines[:-1]:
        cred_string += f'"{cred_line}\\n" \\\n'
    cred_string += f'"{cred_lines[-1]}\\n"\n'

    return cred_string


def prepare_priv_key_string(priv_key_file):
    return f'{CREDS_IDENTIFIER_MAP[CredentialType.PRIV_KEY]} \\\n{prepare_cred_string(priv_key_file)}'


def prepare_cert_string(cert_file):
    return f'{CREDS_IDENTIFIER_MAP[CredentialType.CERT]} \\\n{prepare_cred_string(cert_file)}'


def prepare_root_ca_string(root_ca_file):
    return f'{CREDS_IDENTIFIER_MAP[CredentialType.ROOT_CA]} \\\n{prepare_cred_string(root_ca_file)}'


def prepare_endpoint_string(endpoint):
    return f'{CREDS_IDENTIFIER_MAP[CredentialType.ENDPOINT]} "{endpoint}"\n'


def prepare_thing_name_string(thing_name):
    return f'{CREDS_IDENTIFIER_MAP[CredentialType.THING_NAME]} "{thing_name}"\n'


def replace_identifier_with_string(content_lines, identifier, string):
    pattern = re.compile(f'^{identifier} .*$')

    modified_content_lines = []
    for line in content_lines:
        if pattern.match(line):
            modified_content_lines.append(string)
        else:
            modified_content_lines.append(line)

    return modified_content_lines


def generate_demo_config_file(priv_key_string,
                              cert_string,
                              root_ca_string,
                              thing_name_string,
                              endpoint_string):
    with open(DEMO_CONFIG_FILE_PATH, 'r') as f:
        demo_config_lines = f.readlines()

    modified_demo_config_lines = replace_identifier_with_string(demo_config_lines,
                                                                CREDS_IDENTIFIER_MAP[CredentialType.PRIV_KEY],
                                                                priv_key_string)
    modified_demo_config_lines = replace_identifier_with_string(modified_demo_config_lines,
                                                                CREDS_IDENTIFIER_MAP[CredentialType.CERT],
                                                                cert_string)
    modified_demo_config_lines = replace_identifier_with_string(modified_demo_config_lines,
                                                                CREDS_IDENTIFIER_MAP[CredentialType.ROOT_CA],
                                                                root_ca_string)
    modified_demo_config_lines = replace_identifier_with_string(modified_demo_config_lines,
                                                                CREDS_IDENTIFIER_MAP[CredentialType.THING_NAME],
                                                                thing_name_string)
    modified_demo_config_lines = replace_identifier_with_string(modified_demo_config_lines,
                                                                CREDS_IDENTIFIER_MAP[CredentialType.ENDPOINT],
                                                                endpoint_string)

    with open(GENERATED_DEMO_CONFIG_FILE_PATH, 'w') as f:
        f.writelines(modified_demo_config_lines)


def parse_args():
    parser = argparse.ArgumentParser(description='Generate the demo_config.h file.')
    parser.add_argument('-p', '--priv-key-file', type=str, required=True, help='The path (relative or absolute) to Device Private Key file.')
    parser.add_argument('-c', '--cert-file', type=str, required=True, help='The path (relative or absolute) to Device Certificate File.')
    parser.add_argument('-r', '--root-ca-file', type=str, required=True, help='The path (relative or absolute) to the Server Root CA file.')
    parser.add_argument('-t', '--thing-name', type=str, required=True, help='The AWS IoT thing name.')
    parser.add_argument('-e', '--endpoint', type=str, required=True, help='The AWS IoT end point.')

    args = parser.parse_args()
    return args


def main():
    args = parse_args()
    priv_key_string = prepare_priv_key_string(args.priv_key_file)
    cert_string = prepare_cert_string(args.cert_file)
    root_ca_string = prepare_root_ca_string(args.root_ca_file)
    thing_name_string = prepare_thing_name_string(args.thing_name)
    endpoint_string = prepare_endpoint_string(args.endpoint)

    generate_demo_config_file(priv_key_string,
                              cert_string,
                              root_ca_string,
                              thing_name_string,
                              endpoint_string)

    print('===================================')
    print(f'Replace {DEMO_CONFIG_FILE_PATH} with {GENERATED_DEMO_CONFIG_FILE_PATH}.')
    print('===================================')


if __name__ == "__main__":
    main()
