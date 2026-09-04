"""Read-only loopback observer and standalone HTML report generator."""
import argparse
import json
import pathlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = pathlib.Path(__file__).resolve().parent


def read_rows(path):
    result = []
    try:
        for line in path.read_text(encoding='utf-8').splitlines():
            try:
                result.append(json.loads(line))
            except json.JSONDecodeError:
                pass  # incomplete final lines are retried on the next refresh.
    except FileNotFoundError:
        pass
    return result


def snapshot(root):
    events, results, core, baselines = [], [], [], []
    for path in root.rglob('*.jsonl'):
        if path.name == 'scenarios.jsonl':
            continue
        rows = read_rows(path)
        if path.name == 'results.jsonl':
            for row in rows:
                if row.get('baseline'):
                    baselines.append(row)
                else:
                    results.append(row)
        elif path.name == 'events.jsonl':
            for row in rows:
                if row.get('measurement_scope') in ('CORE_WITH_SYNTHETIC_EVIDENCE', 'LABELED_DELTA_FIXTURES'):
                    core.append(row)
                else:
                    events.append(row)
    events.sort(key=lambda e: e.get('timestamp', 0))
    latest = lambda kind: next((e for e in reversed(events) if e.get('event_type') == kind), {})
    flow = next((e for e in reversed(events) if e.get('event_type') in {
        'buffering_gcs_mission', 'authorized_upload_pending', 'authorized_upload_started',
        'px4_item_transfer', 'revision_committed', 'revision_rejected', 'px4_upload_timeout',
        'LOCAL_COMMIT_FAILED_AFTER_PX4_ACCEPT'}), {})
    # values are displayed, never used to authorize or change a mission.
    return dict(status=latest('status'), decision=latest('decision'), flow=flow,
                results=results, core=core, baselines=baselines,
                source=str(root), recent=events[-30:])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--events', type=pathlib.Path, required=True)
    parser.add_argument('--port', type=int, default=8765)
    parser.add_argument('--report', type=pathlib.Path)
    args = parser.parse_args()
    root = args.events.resolve()
    template = (HERE / 'observer.html').read_text(encoding='utf-8')
    if args.report:
        args.report.mkdir(parents=True, exist_ok=True)
        data = snapshot(root)
        payload = json.dumps(data, allow_nan=False).replace('<', '\\u003c')
        report = template.replace('/*REPORT_DATA*/null', payload)
        (args.report / 'index.html').write_text(report, encoding='utf-8')
        (args.report / 'data.json').write_text(json.dumps(data, indent=2, allow_nan=False), encoding='utf-8')
        print(args.report / 'index.html')
        return

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path == '/':
                content, kind = template.encode(), 'text/html; charset=utf-8'
            elif self.path == '/data':
                content, kind = json.dumps(snapshot(root), allow_nan=False).encode(), 'application/json'
            else:
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header('Content-Type', kind)
            self.send_header('Cache-Control', 'no-store')
            self.send_header('X-Content-Type-Options', 'nosniff')
            self.send_header('Content-Length', str(len(content)))
            self.end_headers()
            self.wfile.write(content)

        def log_message(self, *_):
            pass

    print(f'Observer: http://127.0.0.1:{args.port} (read-only)', flush=True)
    ThreadingHTTPServer(('127.0.0.1', args.port), Handler).serve_forever()


if __name__ == '__main__':
    main()
