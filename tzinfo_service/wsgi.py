"""Production WSGI entry point for the Smol Clock tzinfo service."""

from tzinfo_service import create_app

app = create_app()
