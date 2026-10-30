# Smol Clock tzinfo service — installation

These instructions describe the deployed model used on `rubidium`: Ubuntu
22.04, systemd, Python virtual environment, Gunicorn running as `www-data`, a
MaxMind GeoLite2 City database, and Apache or nginx terminating HTTPS in front
of a Unix-domain socket.

The commands below are run from this folder, `tzinfo_service/`, in a checkout
of the repository. The host configuration files are under `deploy/`.

No web-service process runs as root. Root privileges are required only to
install host packages and administrator-controlled systemd/reverse-proxy
configuration.

## 1. Host packages

Install Python virtual-environment support, `geoipupdate`, and the selected
reverse proxy.

For Apache:

```bash
sudo apt update
sudo apt install python3-venv geoipupdate apache2
sudo a2enmod proxy proxy_http ssl
```

For nginx:

```bash
sudo apt update
sudo apt install python3-venv geoipupdate nginx
```

The service also requires the host `zdump`/tzdata installation. On Ubuntu this
is provided by the standard timezone packages. Verify it before continuing:

```bash
command -v zdump
dpkg-query -W tzdata
test -r /usr/share/zoneinfo/leap-seconds.list
```

The special-time endpoint derives UTC-TAI from the tzdata-provided
`/usr/share/zoneinfo/leap-seconds.list`. The service does not download or
hard-code leap-second state. Only `/special?id=tai` needs it. The file
expires about every six months, so keep the `tzdata` package updated, as
the timezone rules need anyway. `/ready` reports an expired file but is not
made unready by it.

## 2. Application directory

Create the application directory owned by the runtime account:

```bash
sudo install -d -o www-data -g www-data -m 0755 /opt/smol-clock-tzinfo
```

Copy these files into it:

```text
tzinfo_service.py
wsgi.py
requirements.txt
tests/
```

For example, from this folder:

```bash
sudo -u www-data cp tzinfo_service.py wsgi.py requirements.txt /opt/smol-clock-tzinfo/
sudo -u www-data cp -a tests /opt/smol-clock-tzinfo/
```

Verify ownership:

```bash
ls -alh /opt/smol-clock-tzinfo
```

Application files and the virtual environment should be owned by
`www-data:www-data`.

## 3. Python virtual environment

Build the venv as `www-data`:

```bash
cd /opt/smol-clock-tzinfo
sudo -u www-data python3 -m venv .venv
sudo -u www-data .venv/bin/python -m pip install --upgrade pip
sudo -u www-data .venv/bin/pip install -r requirements.txt
```

Check the production entry point without starting Gunicorn:

```bash
sudo -u www-data .venv/bin/python - <<'PY'
import wsgi
print(wsgi.app)
for rule in wsgi.app.url_map.iter_rules():
    print(rule)
PY
```

Expected routes include:

```text
/smol_clock/api/v1/tzinfo
/smol_clock/api/v1/health
/smol_clock/api/v1/ready
/smol_clock/api/v1/special
```

## 4. MaxMind credentials and database directory

Create the configuration and state directories:

```bash
sudo install -d -o www-data -g www-data -m 0750 /etc/smol-clock-tzinfo
sudo install -d -o www-data -g www-data -m 0750 /var/lib/smol-clock-tzinfo
```

Copy the credentials template:

```bash
sudo install -o www-data -g www-data -m 0600 \
    deploy/GeoIP.conf.example /etc/smol-clock-tzinfo/GeoIP.conf
```

Edit `/etc/smol-clock-tzinfo/GeoIP.conf` and provide the MaxMind account ID and
license key. The required edition is:

```text
EditionIDs GeoLite2-City
```

Run an initial update as the runtime user:

```bash
sudo -u www-data geoipupdate \
    -f /etc/smol-clock-tzinfo/GeoIP.conf \
    -d /var/lib/smol-clock-tzinfo
```

Verify:

```bash
ls -alh /var/lib/smol-clock-tzinfo/
```

The expected database is:

```text
/var/lib/smol-clock-tzinfo/GeoLite2-City.mmdb
```

owned by `www-data:www-data`.

## 5. systemd application service

Install the unit definition as administrator-controlled host configuration:

```bash
sudo install -o root -g root -m 0644 \
    deploy/systemd/smol-clock-tzinfo.service \
    /etc/systemd/system/smol-clock-tzinfo.service
```

The unit file is root-owned because `www-data` must not be able to alter its own
systemd execution policy. The process launched by the unit still runs as:

```ini
User=www-data
Group=www-data
```

The production WSGI target is deliberately:

```text
wsgi:app
```

`wsgi.py` contains only the stable boundary:

```python
from tzinfo_service import create_app
app = create_app()
```

Reload systemd:

```bash
sudo systemctl daemon-reload
```

Do not start the public service until the reverse proxy is configured if the
host is already using the endpoint in production.

## 6. MaxMind update service and timer

Install both units:

```bash
sudo install -o root -g root -m 0644 \
    deploy/systemd/smol-clock-geoipupdate.service \
    /etc/systemd/system/smol-clock-geoipupdate.service

sudo install -o root -g root -m 0644 \
    deploy/systemd/smol-clock-geoipupdate.timer \
    /etc/systemd/system/smol-clock-geoipupdate.timer

sudo systemctl daemon-reload
```

Test the updater once:

```bash
sudo systemctl start smol-clock-geoipupdate.service
sudo systemctl status smol-clock-geoipupdate.service --no-pager
```

Then enable its timer:

```bash
sudo systemctl enable --now smol-clock-geoipupdate.timer
systemctl list-timers smol-clock-geoipupdate.timer
```

The updater process runs as `www-data:www-data`.

## 7. Reverse proxy

Gunicorn listens only on:

```text
/run/smol-clock-tzinfo/gunicorn.sock
```

Use either `deploy/apache/smol-clock-tzinfo.conf` or
`deploy/nginx/smol-clock-tzinfo.conf` as the basis for the host configuration.

### Apache

The essential proxy mapping is:

```apache
ProxyPass        "/smol_clock/api/v1/" "unix:/run/smol-clock-tzinfo/gunicorn.sock|http://localhost/smol_clock/api/v1/"
ProxyPassReverse "/smol_clock/api/v1/" "unix:/run/smol-clock-tzinfo/gunicorn.sock|http://localhost/smol_clock/api/v1/"
```

TLS/certificate configuration remains part of the surrounding Apache virtual
host. Apache appends the client's address to `X-Forwarded-For`. With
`TZINFO_TRUST_PROXY=1` the service takes the last address in the header, the
one Apache added, so nothing more is needed.

### nginx

The supplied example proxies the same API prefix to the Unix socket and
sets `X-Forwarded-For` to `$remote_addr`. The service takes the last address
in that header, so appending instead would work as well.

TLS/certificate paths in the example must be changed to match the host.

## 8. Start the service

Enable and start Gunicorn:

```bash
sudo systemctl enable --now smol-clock-tzinfo.service
```

Check status and logs:

```bash
systemctl status smol-clock-tzinfo.service --no-pager
journalctl -u smol-clock-tzinfo.service -n 100 --no-pager
```

Successful Gunicorn startup should report a listener similar to:

```text
Listening at: unix:/run/smol-clock-tzinfo/gunicorn.sock
```

and workers should boot without an application-import error.

## 9. Tests

Run the tests as the service account:

```bash
cd /opt/smol-clock-tzinfo
```


```bash
sudo -u www-data .venv/bin/python -m unittest discover -v -s tests -t .
```

All tests must pass before exposing the updated service.

## 10. Endpoint verification

Liveness:

```bash
curl -fsS https://rubidium.darksky.io/smol_clock/api/v1/health
```

Readiness:

```bash
curl -fsS https://rubidium.darksky.io/smol_clock/api/v1/ready
```

GeoIP request:

```bash
curl -fsS https://rubidium.darksky.io/smol_clock/api/v1/tzinfo
```

Coordinate request:

```bash
curl -fsS \
  'https://rubidium.darksky.io/smol_clock/api/v1/tzinfo?lat=51.5074&lon=-0.1278'
```

Special-time geofence request:

```bash
curl -fsS \
  'https://rubidium.darksky.io/smol_clock/api/v1/special?lat=21.4224779&lon=39.8251832'
```

Explicit special request without coordinates:

```bash
curl -fsS \
  'https://rubidium.darksky.io/smol_clock/api/v1/special?id=mecca_time'
```

TAI request without coordinates:

```bash
curl -fsS \
  'https://rubidium.darksky.io/smol_clock/api/v1/special?id=tai'
```

The timezone response should contain `version`, `source`, `zone`, `generated`,
`valid_until`, a one-entry `current` array, and `next`. Current and future state
records use `transition_utc`, not `epoch`.

## 11. Updating an existing installation

For a normal Python/service update:

```bash
sudo systemctl stop smol-clock-tzinfo.service
```

Replace the application files as `www-data`:

```bash
sudo -u www-data cp tzinfo_service.py wsgi.py requirements.txt /opt/smol-clock-tzinfo/
sudo -u www-data rm -rf /opt/smol-clock-tzinfo/tests
sudo -u www-data cp -a tests /opt/smol-clock-tzinfo/
```

If dependencies changed, rebuild or update the venv:

```bash
cd /opt/smol-clock-tzinfo
sudo -u www-data .venv/bin/pip install -r requirements.txt
```

If `smol-clock-tzinfo.service` changed, install the new unit and reload systemd:

```bash
sudo install -o root -g root -m 0644 \
    deploy/systemd/smol-clock-tzinfo.service \
    /etc/systemd/system/smol-clock-tzinfo.service
sudo systemctl daemon-reload
```

Run tests before restart:

```bash
cd /opt/smol-clock-tzinfo
sudo -u www-data .venv/bin/python -m unittest discover -v -s tests -t .
```

Then restart and verify:

```bash
sudo systemctl restart smol-clock-tzinfo.service
systemctl status smol-clock-tzinfo.service --no-pager
journalctl -u smol-clock-tzinfo.service -n 50 --no-pager
```

### WSGI compatibility note

Older service units used either:

```text
tzinfo_service:app
```

or the factory target:

```text
tzinfo_service:create_app()
```

The current repository model uses:

```text
wsgi:app
```

When updating an older installation, `wsgi.py` and the updated systemd unit must
therefore be deployed together.

## 12. Useful diagnostics

Confirm ownership:

```bash
ls -alh /opt/smol-clock-tzinfo
ls -alh /etc/smol-clock-tzinfo
ls -alh /var/lib/smol-clock-tzinfo
```

Confirm unit contents:

```bash
systemctl cat smol-clock-tzinfo.service
```

Confirm the WSGI target imports:

```bash
cd /opt/smol-clock-tzinfo
sudo -u www-data .venv/bin/python -c 'import wsgi; print(wsgi.app)'
```

Follow logs during startup:

```bash
journalctl -fu smol-clock-tzinfo.service
```
