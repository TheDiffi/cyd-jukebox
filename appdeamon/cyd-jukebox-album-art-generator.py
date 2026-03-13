import os
from io import BytesIO

import appdaemon.plugins.hass.hassapi as hass
import requests
from PIL import Image, ImageEnhance


class CydJukeboxAlbumArtGenerator(hass.Hass):
    def initialize(self):
        self.entity = self.args.get("media_player")
        self.base_url = self.args.get("base_url", "")
        self.output_file = self.args.get("output_file", "media_thumb.jpg")
        self.output_path = f"/homeassistant/www/{self.entity}-{self.output_file}"

        if not self.entity:
            self.log("No media_player specified in apps.yaml", level="ERROR")
            return

        # Listen to *all* state changes of this entity and receive full state dict
        # (old/new are full state dicts when attribute='all'). 
        self.listen_state(self.on_media_change, self.entity, attribute="all")

        self.log(
            f"CydJukeboxAlbumArtGenerator initialized for {self.entity}, "
            f"saving to {self.output_path}"
        )

    def on_media_change(self, entity, attribute, old, new, kwargs):
        # old/new are full state dictionaries when attribute="all". 
        old_pic = (
            old.get("attributes", {}).get("entity_picture")
            if isinstance(old, dict)
            else None
        )
        new_pic = (
            new.get("attributes", {}).get("entity_picture")
            if isinstance(new, dict)
            else None
        )

        # Only run when entity_picture actually changes and is non-empty
        if not new_pic or new_pic == old_pic:
            return

        try:
            url = self._resolve_url(new_pic)
            if not url:
                self.log(
                    f"Cannot resolve entity_picture URL from '{new_pic}'",
                    level="WARNING",
                )
                return

            self.log(f"Downloading artwork from {url}")
            resp = requests.get(url, timeout=10)
            resp.raise_for_status()

            img = Image.open(BytesIO(resp.content)).convert("RGB")
            img = img.resize((60, 60), Image.LANCZOS)
            
            
            img = ImageEnhance.Contrast(img).enhance(1.8)    # +30% contrast
            img = ImageEnhance.Color(img).enhance(1.2)        # +40% saturation

            os.makedirs("/homeassistant/www", exist_ok=True)
            img.save(self.output_path, format="JPEG", quality=85)

            self.log(f"Saved 64x64 cover art to {self.output_path}")
        except Exception as e:
            self.log(f"Error updating thumbnail: {e}", level="ERROR")

    def _resolve_url(self, entity_picture: str) -> str | None:
        """
        Turn the entity_picture attribute into a full URL.

        - If it's absolute (http/https), return as-is.
        - If it's a relative path (/api/...), prepend base_url from args.
        """
        if entity_picture.startswith("http://") or entity_picture.startswith("https://"):
            return entity_picture

        if entity_picture.startswith("/"):
            if not self.base_url:
                return None
            return self.base_url.rstrip("/") + entity_picture

        # Unexpected format
        return None
