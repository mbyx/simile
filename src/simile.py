import time

import serial
import serial.tools.list_ports
import streamlit as st
from PIL import Image

if "animations" not in st.session_state:
    st.session_state.animations = {}

if "serial_connection" not in st.session_state:
    st.session_state.serial_connection = None


def find_esp32_port():
    ports = serial.tools.list_ports.comports()
    for p in ports:
        desc = p.description.lower()
        if any(x in desc for x in ["usb serial", "cp210", "ch340", "silicon labs"]):
            return p.device
    return None


def image_to_byte_array(file) -> list[int]:
    """Convert an image to a binary byte array.

    This method only works with images that only has shades of red and
    nothing else. It returns a byte array with a set bit indicating that
    that pixel was red."""
    img = Image.open(file).convert("RGB").resize((8, 8))
    data = []
    for y in range(8):
        byte = 0
        for x in range(8):
            r, _, _ = img.getpixel((x, y))
            if r != 0:
                byte |= 1 << (7 - x)
        data.append(byte)
    return data


def resized_image(file, scale: int = 32) -> Image:
    """Resize an 8x8 image so that it doesn't appear blurry."""
    image = Image.open(file).convert("RGB")
    return image.resize((image.width * scale, image.height * scale), Image.NEAREST)


def create_animation(name: str) -> None:
    if name and name not in st.session_state.animations:
        st.session_state.animations[name] = {"duration": [], "sprites": []}


def remove_animation(name: str) -> None:
    if name in st.session_state.animations:
        st.session_state.animations.pop(name)


def get_serial_connection():
    """Get or create a persistent serial connection"""
    if st.session_state.serial_connection is None:
        port = find_esp32_port()
        if port:
            try:
                st.session_state.serial_connection = serial.Serial(
                    port, 115200, timeout=1
                )

                time.sleep(2)
                return st.session_state.serial_connection
            except Exception as e:
                st.error(f"Error connecting to board: {e}")
                return None
    return st.session_state.serial_connection


def close_serial_connection():
    """Close the serial connection"""
    if st.session_state.serial_connection:
        st.session_state.serial_connection.close()
        st.session_state.serial_connection = None


def send_to_board(data: str) -> bool:
    """Send data to the ESP32 board using persistent connection"""
    ser = get_serial_connection()
    if ser:
        try:
            print(f"Sending to board: {data.strip()}")
            ser.write(data.encode())
            ser.flush()

            import time

            time.sleep(0.1)
            if ser.in_waiting:
                response = ser.read(ser.in_waiting).decode("utf-8", errors="ignore")
                print(f"Board response: {response}")

            return True
        except Exception as e:
            st.error(f"Error communicating with board: {e}")
            close_serial_connection()
            return False
    else:
        st.toast("Unable to connect to board, make sure that it is connected!")
        return False


def upload_animations() -> None:
    """Upload all animations to the board"""
    if not st.session_state.animations:
        st.toast("No animations to upload!")
        return

    send_to_board("CLEAR\n")

    for anim_name, data in st.session_state.animations.items():
        if not data["sprites"]:
            continue

        frame_data = ""
        for sprite, duration in zip(data["sprites"], data["duration"]):
            binary_string = "".join(f"{byte:08b}" for byte in sprite)
            frame_data += f"{binary_string}:{duration} "

        frame_data = frame_data.strip()
        animation_line = f"ANIM:{anim_name}|{frame_data}\n"

        if not send_to_board(animation_line):
            return

    send_to_board("PLAY_ALL\n")
    st.success("Animations uploaded successfully!")


def play_specific_animation(name: str) -> None:
    """Play a specific animation on the board"""
    if send_to_board(f"PLAY:{name}\n"):
        st.success(f"Playing animation: {name}")


def clear_board() -> None:
    """Clear all animations from the board"""
    if send_to_board("CLEAR\n"):
        st.success("Board cleared!")


st.title("Matrix Animator")
st.write(
    "This app lets you communicate with the `simile` hardware device and provide animations to display."
    " To use it, simply create a new animation and upload 8x8 sprite frames for it!"
)

st.subheader("Connection")
with st.container(border=True):
    col1, col2 = st.columns([0.1, 0.5], vertical_alignment="center")
    with col1:
        if st.button("🔌 Connect"):
            close_serial_connection()
            get_serial_connection()
        if st.button("🔌 Disconnect"):
            close_serial_connection()
    with col2:
        if st.session_state.serial_connection:
            st.success("Connected to ESP32")
        else:
            st.error("Not connected")

st.subheader("Animation")
with st.container(border=True):

    animation_name: str = st.text_input(
        "Enter a name for the new animation:", key="new_anim_name"
    )

    button_col1, button_col2, button_col3, button_col4, button_col5 = st.columns(5)
    with button_col1:
        st.button(
            "**Create Animation**",
            on_click=create_animation,
            args=[animation_name],
            disabled=not animation_name
            or animation_name in st.session_state.animations,
        )
    with button_col2:
        st.button(
            "**Upload Animations**",
            on_click=upload_animations,
            type="primary",
            disabled=not st.session_state.animations,
        )
    with button_col3:
        if st.button("**Clear Board Animations**"):
            clear_board()
    with button_col4:
        if st.button("**Debug Board Info**"):
            send_to_board("DEBUG\n")
    with button_col5:
        if st.button("**Upload Test Pattern**"):
            test_pattern = "11111111" * 8
            send_to_board(f"ANIM:TEST|{test_pattern}:1.0\n")
            send_to_board("PLAY:TEST\n")

for i, (anim_name, data) in enumerate(st.session_state.animations.items()):
    with st.container(border=True):
        header_col1, header_col2, header_col3 = st.columns([0.6, 0.2, 0.2])
        with header_col1:
            st.subheader(f"🎬 {anim_name}")
        with header_col2:
            st.button(
                "Play Animation",
                key=f"play_{anim_name}",
                on_click=play_specific_animation,
                args=[anim_name],
                disabled=not data["sprites"],
            )
        with header_col3:
            st.button(
                "Remove Animation",
                key=f"remove_{anim_name}",
                on_click=remove_animation,
                args=[anim_name],
                type="secondary",
            )

        sprites = st.file_uploader(
            f"Upload frames for {anim_name}:",
            ["jpg", "jpeg", "png"],
            key=f"uploader_{anim_name}",
            accept_multiple_files=True,
            help="Upload 8x8 images with red pixels for the LED matrix",
        )

        if sprites:
            data["sprites"] = []
            data["duration"] = []

            for sprite in sprites:
                byte_array = image_to_byte_array(sprite)
                data["sprites"].append(byte_array)

        if sprites:
            st.write(f"**Frames ({len(sprites)} total):**")

            frames_per_row = 8
            for row_start in range(0, len(sprites), frames_per_row):
                row_sprites = sprites[row_start : row_start + frames_per_row]
                cols = st.columns(len(row_sprites))

                for col, sprite in zip(cols, row_sprites):
                    with col:
                        st.image(
                            resized_image(sprite),
                            width=64,
                            output_format="PNG",
                            channels="RGB",
                            clamp=True,
                            caption=sprite.name,
                        )

                        duration = st.number_input(
                            "Duration (s)",
                            value=1.0 / len(sprites),
                            key=f"duration_{anim_name}_{sprite.name}",
                            min_value=0.1,
                            max_value=10.0,
                            step=0.1,
                            format="%.1f",
                        )
                        data["duration"].append(duration)

        elif data["sprites"]:
            st.info(f"Animation has {len(data['sprites'])} frames ready to upload.")

if st.session_state.animations:
    total_frames = sum(
        len(anim["sprites"]) for anim in st.session_state.animations.values()
    )
    st.info(
        f"Total: {len(st.session_state.animations)} animations, {total_frames} frames"
    )
else:
    st.info("💡 Create your first animation to get started!")
