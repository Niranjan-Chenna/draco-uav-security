import json
import pathlib
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request

root = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(root / 'evaluation'))
from observer import snapshot

raw = root / 'evaluation/results/raw'
raw.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(dir=raw) as temporary:
    directory = pathlib.Path(temporary)
    event = dict(event_type='status', timestamp=1, principal_id='</script><script>bad()</script>')
    failure = dict(event_type='LOCAL_COMMIT_FAILED_AFTER_PX4_ACCEPT', timestamp=2,
                   px4_ack_result=0)
    (directory / 'events.jsonl').write_text(
        json.dumps(event) + '\n' + json.dumps(failure) + '\n' + '{incomplete')
    assert snapshot(directory)['status']['principal_id'] == event['principal_id']
    assert snapshot(directory)['flow'] == failure
    report = directory / 'report'
    subprocess.run([sys.executable, str(root / 'evaluation/observer.py'), '--events', str(directory),
                    '--report', str(report)], check=True, capture_output=True)
    html = (report / 'index.html').read_text()
    assert 'const embedded={' in html
    assert event['principal_id'] not in html
    assert '\\u003c/script>' in html
    process = subprocess.Popen([sys.executable, str(root / 'evaluation/observer.py'), '--events', str(directory),
                                '--port', '18765'], stdout=subprocess.DEVNULL)
    try:
        for attempt in range(30):
            try:
                with urllib.request.urlopen('http://127.0.0.1:18765/data', timeout=1) as response:
                    data = json.load(response)
                break
            except urllib.error.URLError:
                time.sleep(.1)
        else:
            raise AssertionError('observer did not start')
        assert data['status']['principal_id'] == event['principal_id']
        for path, method, status in [('/data', 'POST', 501), ('/../config/sitl_policy.conf', 'GET', 404)]:
            try:
                urllib.request.urlopen(urllib.request.Request('http://127.0.0.1:18765' + path, method=method))
                raise AssertionError('observer accepted a forbidden operation')
            except urllib.error.HTTPError as error:
                assert error.code == status
    finally:
        process.terminate()
        process.wait(timeout=5)
print('observer: JSON, partial-write tolerance, safe HTML embedding, GET-only routes passed')
