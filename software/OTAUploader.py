#!/usr/bin/python3
import sys
import requests
from requests_toolbelt import MultipartEncoder, MultipartEncoderMonitor
import argparse
import json
import time
import os
FIELD_NAME = "ota"
UPLOAD_URI = "/api/v1/ota/"
REBOOT_URI = "/api/v1/post/reboot"
LOGIN_URI = "/api/v1/login/login"
COOKIE_NAME = "PS_sid"
login = {"login" :{"user" : "admin", "passwd" : "password"}}

class OTAUploader:
    def __init__(self, filename, host):
        self.filename = filename
        self.host = host

    def progress(self, count, total, status=''):
        #https://gist.github.com/vladignatyev/06860ec2040cb497f0f3
        bar_len = 60
        filled_len = int(round(bar_len * count / float(total)))

        percents = round(100.0 * count / float(total), 1)
        bar = '=' * filled_len + '-' * (bar_len - filled_len)

        sys.stdout.write('[%s] %s%s ...%s\r' % (bar, percents, '%', status))
        sys.stdout.flush() # As suggested by Rom Ruben (see: http://stackoverflow.com/questions/3173320/text-progress-bar-in-the-console/27871113#comment50529068_27871113)

    def progress_callback(self, monitor):
        self.progress(monitor.bytes_read, monitor.len, status='Uploading OTA')

    def login(self, login_url):
        request = requests.post(login_url, data=json.dumps(login) )
        try:
            return request.cookies[COOKIE_NAME]
        except:
            return None
    def upload(self, upload_url, cookie):
        with open(self.filename, 'rb') as f:
            e = MultipartEncoder(
                fields={'ota': ('ota', f.read())}
                )
            m = MultipartEncoderMonitor(e, self.progress_callback)
            request = requests.post(upload_url, data=m, cookies= {COOKIE_NAME : cookie}, headers={'Content-Type': e.content_type} )
            if request.status_code == 200:
                return 0
            else:
                return -1

    def reboot(self, reboot_url, cookie):
        request = requests.get(reboot_url, cookies= {COOKIE_NAME : cookie}, data="!",)
        if request.status_code == 200:
            return 0
        return -1

    def do_ota(self):
        upload_url = "http://" + self.host + UPLOAD_URI
        reboot_url = "http://" + self.host + REBOOT_URI
        login_url = "http://" + self.host + LOGIN_URI

        session_cookie = self.login(login_url)
        if(session_cookie == None):
            print("Login failed, bailing out")
            return -1
        print("Login successfull, now attempting upload. This may take ~10 seconds")
        upload_result = self.upload(upload_url, session_cookie)
        if(upload_result):
            print("Upload failed")
            return -1
        else:
            print("Upload successfull, rebooting")

        reboot_result = self.reboot(reboot_url, session_cookie)
        if reboot_result:
            print("Reboot failed")
            return -1
        else:
            print("Reboot successfull!")
            return 0



def __main__():
    parser = argparse.ArgumentParser(description='Upload firmware to powersocket using OTA')
    parser.add_argument('-f', '--file', help="The file to flash.")
    parser.add_argument('-ht', '--host', help="The host(s) to upload to.", default = "192.168.0.1", nargs = '+')
    args = parser.parse_args()
    for host in args.host:
        ota = OTAUploader(args.file, host)
        err = ota.do_ota()
        if(err): return err



if __name__ == "__main__":
	__main__()
