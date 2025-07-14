from time import sleep, time
import signal
import sys
from timezonefinder import TimezoneFinder
from datetime import datetime, timezone
import zoneinfo  # use pytz if Python < 3.9
import maxminddb
import logging
from logging.handlers import RotatingFileHandler
import colorlog
import subprocess
import re
from pprint import pprint


log_format = colorlog.ColoredFormatter(
        "%(asctime)s %(log_color)s[%(levelname)s]%(reset)s %(name)s: %(message)s",
        datefmt="%Y-%m-%dT%H:%M:%S",
        log_colors={
        'DEBUG': 'cyan',
        'INFO': 'green',
        'WARNING': 'yellow',
        'ERROR': 'red',
        'CRITICAL': 'red,bg_white',
        })

log_file_format = logging.Formatter(
        fmt="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S"
        )

log_level = logging.INFO

log_file = 'tzinfo_query.log'

file_handler = RotatingFileHandler(log_file, maxBytes=10000000, backupCount=10)
file_handler.setFormatter(log_file_format)

handler = colorlog.StreamHandler()
handler.setFormatter(log_format)

logger = colorlog.getLogger(__name__)
logger.addHandler(handler)
logger.addHandler(file_handler)
logger.setLevel(log_level)

# Path to the GeoLite2-City.mmdb database
db_path = 'GeoLite2-City.mmdb'

# IP address to look up
ip_address = '103.152.127.0'

def parse_zdump_output(output):
  transition_data = []

  zdump_re = re.compile(
        r"^(?P<zone>\S+)\s+"
        r"(?P<utc_date>\w{3} \w{3}\s+\d{1,2} \d{2}:\d{2}:\d{2} \d{4}) UT = "
        r"(?P<local_date>\w{3} \w{3}\s+\d{1,2} \d{2}:\d{2}:\d{2} \d{4}) "
        r"(?P<abbr>\S+) isdst=(?P<isdst>[01]) gmtoff=(?P<offset>-?\d+)"
    )

  for line in output.splitlines():
    match = zdump_re.match(line.strip())
    if match:
      data = match.groupdict()
      utc_dt = datetime.strptime(data["utc_date"], "%a %b %d %H:%M:%S %Y")
      local_dt = datetime.strptime(data["local_date"], "%a %b %d %H:%M:%S %Y")

      transition_data.append({
        "timezone": data["zone"],
        "utc_time": utc_dt.isoformat(),
        "epoch": int(utc_dt.timestamp()),
        "local_time": local_dt.isoformat(),
        "abbreviation": data["abbr"],
        "isdst": bool(int(data["isdst"])),
        "offset_seconds": int(data["offset"])
      })

  return transition_data

def get_zdump_transitions(zone: str, start_ts: int, end_ts: int):
  try:
    result = subprocess.run(
      ["zdump", "-V", "-t", f"{start_ts},{end_ts}", zone],
      capture_output=True,
      text=True,
      check=True
    )
    return parse_zdump_output(result.stdout)

  except subprocess.CalledProcessError as e:
    raise RuntimeError(f"zdump failed: {e.stderr}") from e


def get_next_dst_transition(tz_str: str):
  start = int(time())
  end = start + 31536000 # + 1 year

  output = {}
  transitions = get_zdump_transitions(tz_str, start, end)
  output['zone'] = tz_str
  tz = zoneinfo.ZoneInfo(tz_str)
  output['offset'] = datetime.now(tz).utcoffset().seconds

  if len(transitions) >= 2:
    output['dst'] = transitions[0]['isdst']
    output['next'] = transitions[1]['epoch']

    if(output['dst'] == False):
      output['dst_offset'] = transitions[1]['offset_seconds'] - transitions[0]['offset_seconds']

    else:
      output['dst_offset'] = transitions[0]['offset_seconds'] - transitions[1]['offset_seconds']

  else:
    output['dst'] = False
    output['dst_offset'] = 0
    output['next'] = 0

  return output['next']


def main():
  logger.info("Registering SIGINT handler")
  signal.signal(signal.SIGINT, signal_handler)

  logger.info("Loading timezone shapefile")
  tf = TimezoneFinder()

  logger.info("Loading GeoIP database")
  geoip_reader = maxminddb.open_database(db_path)

  geoip_data = geoip_reader.get(ip_address)

  if geoip_data and 'location' in geoip_data:
    lat_geoip = geoip_data['location'].get('latitude')
    lon_geoip = geoip_data['location'].get('longitude')

    print(f"GeoIP:")
    print(f"IP: {ip_address}")
    print(f"Latitude: {lat_geoip}, Longitude: {lon_geoip}")
    timezone_str = tf.timezone_at(lat=lat_geoip, lng=lon_geoip)  # e.g., 'America/New_York'
    print(f"IANA Timezone: {timezone_str}")
    tz = zoneinfo.ZoneInfo(timezone_str)
    utc = datetime.now(timezone.utc).replace(microsecond=0)
    utc_iso_str = utc.replace(tzinfo=None).isoformat()
    now = datetime.now(tz).replace(microsecond=0)
    now_iso_str = now.replace(tzinfo=None).isoformat()
    utc_offset = now.utcoffset()
    dst_offset = now.dst()
    is_dst = bool(dst_offset and dst_offset.total_seconds() != 0)
    next_dst = datetime.fromtimestamp(get_next_dst_transition(timezone_str))
    next_dst_iso_str = next_dst.replace(tzinfo=None).isoformat()

    print(f"UTC: {utc_iso_str}")
    print(f"Local: {now_iso_str}")
    print(f"DST Offset: {dst_offset}")
    print(f"DST in effect: {is_dst}")
    print(f"UTC Offset: {utc_offset}")
    print(f"Next DST transition: {next_dst_iso_str}")

  else:
    print("Location data not found.")

  # Latitude and Longitude
  lat_gnss = -72.011578
  lon_gnss = 2.536802

  print(f"\nGNSS:")
  print(f"Latitude: {lat_gnss}, Longitude: {lon_gnss}")
  timezone_str = tf.timezone_at(lat=lat_gnss, lng=lon_gnss)  # e.g., 'America/New_York'
  print(f"IANA Timezone: {timezone_str}")
  tz = zoneinfo.ZoneInfo(timezone_str)
  utc = datetime.now(timezone.utc).replace(microsecond=0)
  utc_iso_str = utc.replace(tzinfo=None).isoformat()
  now = datetime.now(tz).replace(microsecond=0)
  now_iso_str = now.replace(tzinfo=None).isoformat()
  utc_offset = now.utcoffset()
  dst_offset = now.dst()
  is_dst = bool(dst_offset and dst_offset.total_seconds() != 0)
  next_dst = datetime.fromtimestamp(get_next_dst_transition(timezone_str))
  next_dst_iso_str = next_dst.replace(tzinfo=None).isoformat()

  print(f"UTC: {utc_iso_str}")
  print(f"Local: {now_iso_str}")
  print(f"DST Offset: {dst_offset}")
  print(f"DST in effect: {is_dst}")
  print(f"UTC Offset: {utc_offset}")
  print(f"Next DST transition: {next_dst_iso_str}")

  while True:
    sleep(0.5)

  geoip_reader.close()
  sys.exit(0)


def signal_handler(sig, frame):
  logger.warning("Caught CTRL-C interrupt! Stopping...")
  geoip_reader.close()
  sys.exit(0)


if __name__ == '__main__':
    main()
