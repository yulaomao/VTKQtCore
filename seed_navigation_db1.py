from __future__ import annotations

import argparse
import json
import sys
import time


def build_transform(node_id: str, tx: float, ty: float, tz: float, sequence: int) -> dict:
    return {
        "nodeId": node_id,
        "matrixToParent": [
            1.0,
            0.0,
            0.0,
            tx,
            0.0,
            1.0,
            0.0,
            ty,
            0.0,
            0.0,
            1.0,
            tz,
            0.0,
            0.0,
            0.0,
            1.0,
        ],
        "timestampMs": int(time.time() * 1000),
        "quality": {
            "trackingState": "stable",
            "rmsErrorMm": round(0.08 + sequence * 0.01, 3),
            "covarianceDiag": [0.02 + sequence * 0.001, 0.02, 0.03],
        },
        "tags": ["navigation", "batch-read", f"seq-{sequence}"],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="向本地 Redis db1 写入 navigation 复杂哈希数据")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=6379)
    parser.add_argument("--db", type=int, default=1)
    args = parser.parse_args()

    try:
        import redis
    except ImportError:
        print("缺少 redis 包，请先安装 redis>=5", file=sys.stderr)
        return 2

    client = redis.Redis(host=args.host, port=args.port, db=args.db, decode_responses=True)
    client.ping()

    navigation_state = {
        "navigating": True,
        "status": "Tracking",
        "position": [12.34, -5.67, 88.9],
        "worldMatrix": [
            1.0,
            0.0,
            0.0,
            120.5,
            0.0,
            1.0,
            0.0,
            -32.25,
            0.0,
            0.0,
            1.0,
            410.75,
            0.0,
            0.0,
            0.0,
            1.0,
        ],
        "meta": {
            "patient": {"id": "P-042", "side": "left"},
            "tracker": {"name": "aurora", "firmware": "2.3.7"},
            "flags": ["calibrated", "sterile-cover", "demo"],
        },
    }

    transforms = {
        "world": build_transform("navigation-world-transform", 120.5, -32.25, 410.75, 1),
        "reference": build_transform("navigation-reference-transform", 18.0, 9.5, 2.25, 2),
        "patient": build_transform("navigation-patient-transform", 6.2, 3.4, -1.5, 3),
        "instrument": build_transform("navigation-instrument-transform", 42.8, -11.0, 85.3, 4),
        "guide": build_transform("navigation-guide-transform", 44.1, -9.8, 97.0, 5),
        "tip": build_transform("navigation-tip-transform", 45.0, -9.1, 104.2, 6),
    }

    pipeline = client.pipeline()
    pipeline.hset("state.navigation", mapping={"latest": json.dumps(navigation_state, ensure_ascii=False)})
    pipeline.hset(
        "demo:navigation:transform",
        mapping={name: json.dumps(payload, ensure_ascii=False) for name, payload in transforms.items()},
    )
    pipeline.execute()

    print(f"已写入 Redis {args.host}:{args.port} db={args.db}")
    print("  - state.navigation.latest")
    for field_name in transforms:
        print(f"  - demo:navigation:transform.{field_name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())