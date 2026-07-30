import argparse
import statistics
import struct
import time

from pyocd.core.helpers import ConnectHelper


RAW_SEQUENCE_ADDR = 0x20000044
RAW_DISTANCE_ADDR = 0x20000050
RAW_AZIMUTH_ADDR = 0x20000054
FILTERED_AZIMUTH_ADDR = 0x20000008
KALMAN_X_ADDR = 0x20000058
HAVE_FRAME_ADDR = 0x20000002


def signed16(value):
    return value - 0x10000 if value & 0x8000 else value


def read_float32(target, address):
    value = target.read32(address)
    return struct.unpack("<f", struct.pack("<I", value))[0]


def describe(name, values):
    print(
        f"{name}: count={len(values)} mean={statistics.fmean(values):.2f} "
        f"stdev={statistics.pstdev(values):.2f} "
        f"min={min(values):.2f} max={max(values):.2f}"
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seconds", type=float, default=15.0)
    parser.add_argument("--probe", default="FS-00000000")
    args = parser.parse_args()

    session = ConnectHelper.session_with_chosen_probe(
        unique_id=args.probe,
        target_override="stm32f103rc",
        options={
            "connect_mode": "attach",
            "frequency": 100000,
            "halt_on_connect": False,
            "resume_on_disconnect": True,
        },
    )
    if session is None:
        raise RuntimeError("CMSIS-DAP probe not found")

    raw_values = []
    distance_values = []
    filtered_values = []
    kalman_values = []
    last_sequence = None
    deadline = time.monotonic() + args.seconds

    with session:
        target = session.board.target
        if target.get_state().name == "HALTED":
            target.resume()

        while time.monotonic() < deadline:
            if target.read8(HAVE_FRAME_ADDR):
                sequence = target.read16(RAW_SEQUENCE_ADDR)
                if sequence != last_sequence:
                    last_sequence = sequence
                    raw = signed16(target.read16(RAW_AZIMUTH_ADDR))
                    distance = target.read32(RAW_DISTANCE_ADDR)
                    filtered = signed16(target.read16(FILTERED_AZIMUTH_ADDR))
                    kalman = read_float32(target, KALMAN_X_ADDR)
                    raw_values.append(raw)
                    distance_values.append(distance)
                    filtered_values.append(filtered)
                    kalman_values.append(kalman)
                    print(
                        f"seq={sequence:5d} distance={distance:4d}cm raw={raw:+4d} "
                        f"filtered={filtered:+4d} kalman={kalman:+7.2f}"
                    )
            time.sleep(0.05)

    if not raw_values:
        raise RuntimeError("No live UWB frames sampled")
    describe("raw", raw_values)
    describe("distance_cm", distance_values)
    describe("filtered", filtered_values)
    describe("kalman", kalman_values)


if __name__ == "__main__":
    main()
