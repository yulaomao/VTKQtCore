from __future__ import annotations

import argparse
import pathlib
import sys


def main() -> int:
    workspaceRoot = pathlib.Path(__file__).resolve().parent
    sys.path.insert(0, str(workspaceRoot / "python_src"))

    parser = argparse.ArgumentParser(description="PyQt 模拟服务端")
    parser.add_argument("--server-kind", choices=["cpp-test", "modern-mock"], default="cpp-test")
    parser.add_argument("--redis-host", default="127.0.0.1")
    parser.add_argument("--redis-port", type=int, default=6379)
    parser.add_argument("--redis-db", type=int, default=0)
    parser.add_argument("--software-type", default="default")
    brokerGroup = parser.add_mutually_exclusive_group()
    brokerGroup.add_argument("--embedded-broker", dest="use_embedded_broker", action="store_true")
    brokerGroup.add_argument("--no-embedded-broker", dest="use_embedded_broker", action="store_false", help=argparse.SUPPRESS)
    parser.set_defaults(use_embedded_broker=False)
    args = parser.parse_args()

    from config.runtime_config import RedisConnectionConfig

    redisConnectionConfig = RedisConnectionConfig(
        host=args.redis_host,
        port=args.redis_port,
        db=args.redis_db,
        autoConnect=True,
    )

    if args.server_kind == "modern-mock":
        from server.main import main as serverMain

        return serverMain(redisConnectionConfig=redisConnectionConfig)

    from server.cpp_test_server import main as serverMain

    return serverMain(
        redisConnectionConfig=redisConnectionConfig,
        useEmbeddedBroker=args.use_embedded_broker,
        softwareType=args.software_type,
    )


if __name__ == "__main__":
    raise SystemExit(main())