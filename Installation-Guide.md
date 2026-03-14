# Installation

There two essential steps to get this working: **Configuring and flashing ESPHome** and **Setting up Appdeamon** to generate a scaled down version of the song thumbnail for the cyd-jukebox. This is needed because the esp on the cyd does not have enough ram for the fullscale image.
_Note: If your esp has extra psram for some reason, you can skip this and render the full image instead. Just enter the image url in the `online_image` config_

- (Clone or download this repo)
- EspHome
  - Adjusting the config and retrieving a long term access token
  - Flashing EspHome onto the cyd
  - Adding it to HA and enabling "Allow this device to execute actions"
- The Album Cover Image
  - Install the AppDeamon Plugin on HA
  - Add the required libraries in the Appdeamon Addon Config
  - Add the script under /appdeamon to Appdeamon
  - Restart the Plugin and verify it working

## ESPHOME

### Configuring your Jukebox

Head to /esphome

- copy `config.yaml.example` and rename it to `config.yaml`
- copy `secrets.yaml.example` and remane it to `secrets.yaml`

Open your `config.yaml` and follow the description to replace the example values with your actual ones. The name and display-name can be choosen freely.

Open your `secrets.yaml` and follow the description to replace the example values with your actual ones.

### Flashing

You'll need to flash this esphome config onto the cyd. There are to my knowledge two ways: locally with python and the command line or via the ESPHome Device Builder Addon

If you are comfortable with installing esphome as a python library locally, I would reccomend taking that route.

#### Method 1: Locally

Follow this guide to install esphome locally: <https://esphome.io/guides/installing_esphome/>
Plug in your cyd and ensure you can establish serial connection. You might need to install the relevant UART drivers.
Then navigate to the esphome directory and build the `cyd-jukebox.yaml` config

    cd esphome
    esphome run cyd-jukebox.yaml

It will ask you `Found multiple options for uploading, please choose one:`, select the first option (USB Serial).

Done. Hurray. Easy as cake.

_Note: you can run the esphome run command again later to update the device over the air, so even without needing to connect it via usb!_

#### Method 2: ESPHome Device Builder

    An addiational quest needs completion, before continuing on this path...
    You'll need to copy over the config values you entered into the `esphome/config.yaml`
    earlier into the esphome yaml `esphome/cyd-jukebox-FULL.yaml`.
    At the top of that file you see a section `substitutions`.
    Just replace this section with the one inside of your `config.yaml`.
    This ensure you are not using the example values but your own ones.

Follow this guide to set up the Addon <https://esphome.io/guides/getting_started_hassio/>

- Create a new empty config inside ESPHome builder
- Edit the device/yaml, Remove anything that might be in there
- Copy the full `esphome/cyd-jukebox-FULL.yaml` file contents into there
- save
- Add your secrets you configured in `esphome/secrets.yaml` to the secrets of the esphome device builder. You can find them in the interface on the top right, there is a button "Secrets"
- connect your cyd via usb
- compile and upload the new device
  - [Follow this guide if you've never done this before](https://esphome.io/guides/getting_started_hassio/)

Done! Your CYD should start looking for your wifi and displaying something

### Add to Home Assistant and give it the permissions

As the last step you need to add the device to HA. In HA, head to Settings -> Devices. It should be autodiscovered there, alternatively select the ESPHome integration. The CYD displays its ip in the logs once it has connected to your Wifi.

Then go again to Settings -> Devices, and select the ESPHome Integration. You should see a list off all esphome devices, including your cyd-jukebox. Press the gear to open the config for this device and select "Allow the device to perform Home Assistant Actions"

![Find the Device](assets/esphome-perms-1.png)
![Give it the permission](assets/esphome-perms-2.png)

Hurray, the device is not operational.

## Optional: Curate your Playlists Page

The playlist page offers a way to start playing one of your personally curated playlists.
To do that, head over to `esphome/cyd-playlists-page.yaml` if you flashed it locally, or find the `Scrollable playlist list` Section in your ESPHome Device Builder yaml config.

Follow the steps outlined there to add your own playlists


## Appdeamon (for the cover art)
TODO
