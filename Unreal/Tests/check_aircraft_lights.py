"""Exercise the Unreal light components through Fly2Gether's property port."""
import argparse
import time

from fly2gether_custom import Props


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True)
    client = Props(parser.parse_args().host)
    master = "/controls/switches/master-bat"
    alternator = "/controls/switches/master-alt"
    switches = {
        "landing": "landing-lights",
        "taxi": "taxi-light",
        "navigation": "nav-lights",
        "beacon": "beacon",
        "strobe": "strobe",
    }
    paths = {name: "/controls/lighting/" + switch for name, switch in switches.items()}
    restore = {path: client.get(path) for path in [master, alternator, *paths.values()]}
    try:
        client.set(alternator, 0)
        client.set(master, 1)
        for path in paths.values():
            client.set(path, 1)
        time.sleep(.15)

        def visible(name):
            return client.get("/sim/oneclickpilot/lights/" + name)

        for name in ("landing", "taxi", "navigation"):
            assert visible(name) == 1, name
        observed = {name: set() for name in ("beacon", "strobe")}
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and any(len(values) < 2 for values in observed.values()):
            for name, values in observed.items():
                values.add(visible(name))
            time.sleep(.02)
        assert all(values == {0, 1} for values in observed.values()), observed

        client.set(master, 0)
        time.sleep(.15)
        for name in paths:
            assert visible(name) == 0, name
        print("PASS: landing, taxi, navigation, flashing beacon/strobes, and master battery cutout")
    finally:
        for path, value in restore.items():
            client.set(path, value)
        client.close()


if __name__ == "__main__":
    main()
