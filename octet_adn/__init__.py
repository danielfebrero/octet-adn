"""Octet-ADN : génomes en octets, VM déterministe, 10 éprouvettes d'évolution."""
from pathlib import Path
import hashlib

__version__ = "0.2.0"


def engine_version() -> str:
    """Empreinte du code moteur (la « popote ») : change dès qu'un .py change."""
    h = hashlib.sha1()
    root = Path(__file__).parent
    for p in sorted(root.rglob("*.py")):
        h.update(p.relative_to(root).as_posix().encode())
        h.update(p.read_bytes())
    return h.hexdigest()[:10]
