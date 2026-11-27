#!/usr/bin/env python3
"""Isolated 3311 runtime IP-list UI regression, including firewall callbacks.

Usage: python3 test/runtime_ip_lists_regression.py KANGLE WEBADMIN_DIR
No live configuration or system firewall is changed. Firewall commands append
to a private fixture log, and the WHM seed package is loopback-only.
"""
import argparse
import base64
import concurrent.futures
import hashlib
import http.client
import json
from pathlib import Path
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import urllib.parse
import xml.etree.ElementTree as ET
from runtime_fixture import copy_server, stop_server


def free_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kangle')
    parser.add_argument('webadmin')
    args = parser.parse_args()
    checks = 0

    def check(condition, label):
        nonlocal checks
        assert condition, label
        checks += 1
        print('PASS: ' + label, flush=True)

    with tempfile.TemporaryDirectory(prefix='kangle-runtime-lists-') as directory:
        root = Path(directory)
        for name in ('bin', 'etc', 'ext', 'var', 'tmp', 'www', 'webadmin'):
            (root / name).mkdir()
        binary = copy_server(str(Path(args.kangle).resolve()), str(root / 'bin'))
        for name in ('lang.xml', 'lang_en.xml', 'lang_zh.xml', 'lang_tw.xml'):
            shutil.copy2(str(Path(args.webadmin) / name), str(root / 'webadmin' / name))
        for name in ('ready', 'g', 'v', 'w', 'check-white'):
            (root / 'www' / name).write_text('ready')
        (root / 'www/seed.whm').write_text("<package><extend name='core' type='core'/><call name='report_ip' extend='core' scope='public'/><call name='black_list' extend='core' scope='public'/></package>")
        helper = root / 'bin/firewall.py'
        helper.write_text("import sys\nwith open(sys.argv[1], 'a') as f: f.write(sys.argv[2]+' '+sys.argv[3]+'\\n')\n")
        admin_port, web_port = free_port(), free_port()
        origin = 'http://127.0.0.1:' + str(admin_port)
        config = ET.Element('config')
        ET.SubElement(config, 'worker_thread').text = '2'
        ET.SubElement(config, 'lang').text = 'en'
        ET.SubElement(config, 'listen', ip='127.0.0.1', port=str(admin_port), type='manage')
        ET.SubElement(config, 'listen', ip='127.0.0.1', port=str(web_port), type='http')
        ET.SubElement(config, 'admin', user='admin', password='fixture-only', crypt='plain', auth_type='Basic', admin_ips='127.0.0.1')
        ET.SubElement(config, 'timeout', rw='10', connect='2')
        ET.SubElement(config, 'cache', default='0', disk='0')
        vhs = ET.SubElement(config, 'vhs')
        ET.SubElement(vhs, 'mime_type', ext='*', type='text/plain')
        firewall_log = root / 'firewall.log'
        command = '{} {} {} '.format(sys.executable, helper, firewall_log)
        ET.SubElement(config, 'firewall', bl_time='0', wl_time='0',
                      block_ip_cmd=command + 'block ${ip}', unblock_ip_cmd=command + 'unblock ${ip}')
        ET.SubElement(config, 'api', name='whm', file='buildin:whm')
        request_rules = ET.SubElement(config, 'request', action='allow')
        table = ET.SubElement(request_rules, 'table', name='BEGIN')
        chain = ET.SubElement(table, 'chain', action='continue')
        ET.SubElement(chain, 'mark_replace_ip', header='X-List-IP')
        chain = ET.SubElement(table, 'chain', action='continue')
        ET.SubElement(chain, 'acl_path', path='/g')
        ET.SubElement(chain, 'mark_black_list')
        chain = ET.SubElement(table, 'chain', action='continue')
        ET.SubElement(chain, 'acl_path', path='/w')
        ET.SubElement(chain, 'mark_white_list')
        chain = ET.SubElement(table, 'chain', action='deny')
        ET.SubElement(chain, 'acl_path', path='/check-white')
        ET.SubElement(chain, 'acl_white_list')
        ET.SubElement(config, 'response', action='allow')
        for name in ('one.test', 'two.test'):
            vh = ET.SubElement(config, 'vh', name=name, doc_root='www', inherit='on')
            ET.SubElement(vh, 'host').text = name
            ET.SubElement(vh, 'map', path='/seed.whm', extend='api:whm', allow_method='*')
            request = ET.SubElement(vh, 'request', action='allow')
            chain = ET.SubElement(ET.SubElement(request, 'table', name='BEGIN'), 'chain', action='continue')
            ET.SubElement(chain, 'acl_path', path='/v')
            ET.SubElement(chain, 'mark_black_list')
        cfg = root / 'etc/config.xml'
        cfg.write_bytes(ET.tostring(config))
        config_hash = hashlib.sha256(cfg.read_bytes()).hexdigest()
        auth = 'Basic ' + base64.b64encode(b'admin:fixture-only').decode()

        def request(path, admin=False, method='GET', body=None, headers=None, host='one.test', ip=None, authenticated=True):
            hdr = {'Host': '127.0.0.1:' + str(admin_port) if admin else host}
            if admin and authenticated:
                hdr['Authorization'] = auth
            if ip:
                hdr['X-List-IP'] = ip
            if headers:
                hdr.update(headers)
            cn = http.client.HTTPConnection('127.0.0.1', admin_port if admin else web_port, timeout=15)
            try:
                cn.request(method, path, body, hdr)
                response = cn.getresponse()
                return response.status, response.read().decode(errors='replace')
            finally:
                cn.close()

        def counts():
            status, html = request('/main', admin=True)
            assert status == 200
            fields = ('runtime_black_count', 'runtime_global_black_count', 'runtime_vh_black_count', 'runtime_white_count')
            return tuple(int(re.search("id='{}'>([0-9]+)</span>".format(field), html).group(1)) for field in fields)

        def clear(kind, **kwargs):
            headers = kwargs.pop('headers', {'Origin': origin})
            return request('/clear_runtime_ip_list', admin=True, method='POST',
                           body='type=' + kind, headers=headers, **kwargs)

        def seed(ips):
            status, data = request('/seed.whm?whm_call=report_ip', method='POST', body=urllib.parse.urlencode({'ips': ips}))
            assert status == 200 and '200' in data, data

        def black_hit(ip):
            _, data = request('/seed.whm?' + urllib.parse.urlencode({'whm_call': 'black_list', 'a': 'check', 'ip': ip}))
            return ET.fromstring(data).findtext('.//hit') == '1'

        with open(str(root / 'console.log'), 'wb') as log:
            proc = subprocess.Popen([binary, '-n', '-g'], stdout=log, stderr=log)
            try:
                for _ in range(100):
                    try:
                        if request('/ready')[0] == 200:
                            break
                    except OSError:
                        time.sleep(.05)
                else:
                    raise AssertionError('startup failed')
                check(counts() == (0, 0, 0, 0), 'empty runtime counts')
                html = request('/main', admin=True)[1]
                check(html.index('runtime_black_count') < html.index('runtime_white_count') and
                      html.count("action='/clear_runtime_ip_list'") == 2 and "value='all'" not in html,
                      'homepage has exactly the two separate clear actions')
                # runtime_ip_lists in webadmin/lang_en.xml
                section = html[html.index('<h3>IP lists</h3>'):html.index('<h3>load info</h3>')]
                check(html.index('<h3>uptime</h3>') < html.index('<h3>IP lists</h3>') and
                      "type='submit'" not in section and section.count('[<a ') == 2 and
                      "getElementById('clear_runtime_black_form').submit()" in section and
                      "getElementById('clear_runtime_white_form').submit()" in section and
                      'Only runtime entries' not in html,
                      'bracketed clear links below uptime submit hidden POST forms; explanatory line removed')
                request('/g', ip='198.51.100.10')
                request('/g', ip='198.51.100.11')
                for host in ('one.test', 'two.test'):
                    request('/v', host=host, ip='198.51.100.10')
                    request('/w', host=host, ip='198.51.100.10')
                seed('2198.51.100.20,3198.51.100.21,0198.51.100.22,2198.51.100.22,0198.51.100.23,3198.51.100.23,0198.51.100.24,4198.51.100.24')
                check(counts() == (4, 2, 2, 2), 'global/vhost counts exclude fixed and removed entries; whitelist counts domain/IP pairs')
                check(request('/check-white', ip='198.51.100.10')[0] == 403, 'whitelist entry matches before clear')
                clear('black', authenticated=False)
                request('/clear_runtime_ip_list?type=black', admin=True)
                clear('black', headers={})
                clear('black', headers={'Origin': 'http://other.test'})
                clear('black', headers={'Origin': 'http://127.0.0.1:' + str(web_port)})
                check(counts() == (4, 2, 2, 2), 'unauthenticated, GET, missing-origin and cross-origin requests cannot clear lists')
                status, html = clear('black')
                check(status == 200 and "id='ip_list_notice'" in html and 'Entries cleared 4' in html,
                      'blacklist clear reports actual number removed')
                check(counts() == (0, 0, 0, 2), 'global and all vhost runtime blacklists cleared; whitelist preserved')
                check(black_hit('198.51.100.20') and black_hit('198.51.100.22') and
                      not black_hit('198.51.100.21') and not black_hit('198.51.100.23'),
                      'fixed deny/allow entries survive clear, including converted queue entries')
                for _ in range(100):
                    lines = firewall_log.read_text().splitlines() if firewall_log.exists() else []
                    if 'unblock 198.51.100.11' in lines:
                        break
                    time.sleep(.05)
                check(sorted(line for line in lines if line.startswith('unblock ')) ==
                      ['unblock 198.51.100.10', 'unblock 198.51.100.11'],
                      'firewall callbacks unblock exactly the cleared global dynamic IPs')
                check('Entries cleared 0' in clear('black')[1], 'repeated blacklist clear is safe')
                request('/g', ip='198.51.100.12')
                status, html = clear('white', headers={'Referer': origin + '/main'})
                check(status == 200 and 'Entries cleared 2' in html, 'whitelist clear reports actual domain/IP count; Referer fallback works')
                check(counts() == (1, 1, 0, 0), 'whitelist clear preserves blacklist')
                check(request('/check-white', ip='198.51.100.10')[0] == 200, 'cleared whitelist no longer matches')
                with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
                    list(executor.map(lambda i: request('/w', host='one.test', ip='2001:db8::' + str(i % 4 + 1)), range(24)))
                check(counts() == (1, 1, 0, 4), 'whitelist can be repopulated concurrently with IPv6; duplicate entries counted once')
                clear('white')
                clear('white')
                clear('black')
                check(counts() == (0, 0, 0, 0) and proc.poll() is None, 'repopulate/clear cycles remain healthy')
                check(hashlib.sha256(cfg.read_bytes()).hexdigest() == config_hash, 'clearing runtime lists does not alter XML configuration')
            finally:
                stop_server(proc, binary)
        print('PASS: {} checks'.format(checks), flush=True)


if __name__ == '__main__':
    main()
