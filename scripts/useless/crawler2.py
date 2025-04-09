# -*- coding: UTF-8 -*-

import requests, time
from fake_useragent import UserAgent
from lxml import etree
import re

def ua():
    ua=UserAgent()
    headers={
        'User-Agent':ua.random,
    }
    return headers


def get_data():
    url="https://vault.centos.org/7.0.1406/isos/x86_64/"
    html=requests.get(url=url).content.decode('utf-8')
    tree=etree.HTML(html)
    divs=tree.xpath('//a[contains(@href,".iso")]')
    print(len(divs))

    for div in divs:
        href=div.xpath('./@href')[0]
        get_sky(url + href, href)
        time.sleep(2)


def get_real_url(url):
    rs = requests.get(url, headers=ua(), timeout=10)
    print(rs.url)
    return rs.url

def get_sky(url, name):
    down_url=get_real_url(url)
    r=requests.get(url=down_url)
    print(f"Downloading {name} ...", end=' ')
    with open('/home/cyf/dataHDD/CentOS2/'+name, 'wb') as f:
        f.write(r.content)
    print("done!")

if __name__ =='__main__':
    get_data()