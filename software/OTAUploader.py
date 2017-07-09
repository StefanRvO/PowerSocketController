#!/usr/bin/python3
import sys
import requests
import argparse
import json
import time
FIELD_NAME = "ota"
UPLOAD_URI = "/api/v1/ota/"
REBOOT_URI = "/api/v1/post/reboot"
LOGIN_URI = "/api/v1/login/login"
COOKIE_NAME = "PS_sid"
login = {"login" :{"user" : "admin", "passwd" : "password"}}

def do_ota(filename, host):
    url = "http://" + host + UPLOAD_URI
    reboot_url = "http://" + host + REBOOT_URI
    login_url = "http://" + host + LOGIN_URI

    session_cookie = None
    request = requests.post(login_url, data=json.dumps(login) )
    try:
        session_cookie = request.cookies[COOKIE_NAME]
    except KeyError:
        print("Login failed, bailing out")
        return -1
    print("Login successfull, now attempting upload. This may take ~10 seconds")
    with open(filename, 'rb') as f:
        request = requests.post(url, files={'ota': f.read()}, cookies= {COOKIE_NAME : session_cookie} )
        if request.status_code == 200:
            print("Upload successfull, rebooting!")
            request = requests.get(reboot_url, cookies= {COOKIE_NAME : session_cookie}, data="!",)
            if request.status_code == 200:
                print("Reboot successfull!")
                return 0
            else:
                print("Reboot failed")
                return -1

        #else:
        #    print("Upload failed")
        #    return -1


def __main__():
    parser = argparse.ArgumentParser(description='Upload firmware to powersocket using OTA')
    parser.add_argument('-f', '--file', help="The file to flash.")
    parser.add_argument('-ht', '--host', help="The host(s) to upload to.", default = "192.168.0.1", nargs = '+')
    args = parser.parse_args()
    for host in args.host:
        err = do_ota(args.file, host)
        if(err): return err



if __name__ == "__main__":
	__main__()
