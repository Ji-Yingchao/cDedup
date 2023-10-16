import requests
from bs4 import BeautifulSoup
import time
from multiprocessing import  Process

# 下载ubuntu cloud image所有qcow2 daily build
down_folder = '/home/cyf/dataHDD/qcow2/'

def down_one_file(url):
    # make local file name
    local_file_name = url
    local_file_name = local_file_name.replace('/', '-')
    full_name = down_folder+local_file_name

    response = requests.get(url)
    if response.status_code == 200:
        # Open the local file for writing in binary mode
        with open(full_name, 'wb') as file:
            # Write the content from the response to the local file
            file.write(response.content)
        print(f"File downloaded and saved to {full_name}")

def down_from_urls(urls):
    process_list = []
    for url in urls:
        p = Process(target=down_one_file,args=(url,)) #实例化进程对象
        p.start()
        process_list.append(p)

    for p in process_list:
        p.join()

def process_daily_page(url):
    response = requests.get(url)
    downurls = []

    if response.status_code == 200:
        soup = BeautifulSoup(response.text, 'html.parser')
        links = soup.find_all('a')

        for link in links:
            href = link.get('href')
            if href and href[-3:] == 'img': # 只下载qcow2
                downurls.append(url+href)

    down_from_urls(downurls)

def process_version_page(url):
    response = requests.get(url)

    if response.status_code == 200:
        soup = BeautifulSoup(response.text, 'html.parser')
        links = soup.find_all('a')

        for link in links:
            href = link.get('href')
            if href:
                if href == 'current/' or href[:4] == '2019' or href[:4] == '2021' or href[:4] == '2023':
                    time.sleep(0.1)
                    process_daily_page(url+href)
    else:
        print(f"Failed to retrieve the page. Status code: {response.status_code}")

if __name__ == '__main__':
    # urls = ['https://cloud-images.ubuntu.com/trusty/']

    # urls = ['https://cloud-images.ubuntu.com/xenial/']

    urls = ['https://cloud-images.ubuntu.com/bionic/',
            'https://cloud-images.ubuntu.com/focal/',
            'https://cloud-images.ubuntu.com/jammy/',
            'https://cloud-images.ubuntu.com/lunar/',
            'https://cloud-images.ubuntu.com/mantic/']
    
    for url in urls:
        process_version_page(url)