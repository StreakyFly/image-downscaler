
from PIL import Image
from colorama import Fore, Style, init
from shutil import copytree
import os
from tkinter import filedialog, Tk
import time
import traceback


def main():
    print(f"{Fore.LIGHTBLUE_EX}Izberi direktorij s slikami...{Style.RESET_ALL}")
    time.sleep(1)
    Tk().withdraw()
    image_dir = filedialog.askdirectory().replace("/", "\\")
    print("Izbran:", image_dir)
    print(f"{Fore.LIGHTBLUE_EX}Vpiši najdaljšo dolžino slike (v pixlih): {Style.RESET_ALL}")
    max_image_length = int(input(">> "))

    create_backup(image_dir=image_dir)

    checked_count = 0
    resized_count = 0
    saved_space_smt2Jpg = 0
    saved_space_resize = 0
    for files in os.walk(image_dir):
        dir = files[0]
        imgs = files[2]
        for img in imgs:
            if img.lower().endswith(".jpg") or img.lower().endswith(".png") or img.lower().endswith(".bmp"):
                try:
                    r, ss_smt2jpg, ss_resize = resize_image(f"{dir}\\{img}", max_image_length)
                    checked_count += 1
                    resized_count += r
                    saved_space_smt2Jpg += ss_smt2jpg
                    saved_space_resize += ss_resize
                except Exception as e:
                    print(f"{Fore.LIGHTRED_EX}Error: {e}\n {traceback.format_exc()}")

    print(f"\n{Fore.LIGHTMAGENTA_EX}Preverjenih slik: {checked_count}")
    print(f"Zmanjšanih slik: {resized_count} (samo pretvorjene slike iz PNG/BMP v JPG zraven niso štete)\n")

    print(f"{Fore.LIGHTCYAN_EX}{'Prihranjen prostor (PNG/BMP v JPG):'.ljust(44, ' ')} {MBtoGBstr(saved_space_smt2Jpg)}")
    print(f"{Fore.LIGHTCYAN_EX}{'Prihranjen prostor (zmanjšanje rezolucije):'.ljust(44, ' ')} {MBtoGBstr(saved_space_resize)}")
    print(f"{Fore.LIGHTGREEN_EX}{'Skupno prihranjen prostor:'.ljust(44, ' ')} {MBtoGBstr(saved_space_smt2Jpg+saved_space_resize)}")

    print(f"\n{Fore.LIGHTBLUE_EX}Pritisni ENTER za konec.")
    input()


def create_backup(image_dir):
    print(f"{Fore.LIGHTRED_EX}Ali želiš narediti kopijo slik? [Y/N]: {Style.RESET_ALL}")
    backup_yn = input(">> ")
    if backup_yn.lower() in ["n", "no", "ne"]:
        print(f"{Fore.LIGHTRED_EX}Kopija slik NE bo narejena.{Style.RESET_ALL}")
        print(f"{Fore.LIGHTRED_EX}Nadaljevanje brez kopije... {Style.RESET_ALL}")
    elif backup_yn.lower() in ["y", "yes", "d", "da"]:
        dir_split = image_dir.split("\\")
        dir_left = "\\".join(dir_split[:-1])
        dir_original = f"{dir_left}\\{dir_split[-1]}-ORIGINAL"
        copytree(image_dir, dir_original, dirs_exist_ok=True, ignore=ignore_files)  # naredi backup vseh datotek
        print(f"{Fore.LIGHTRED_EX}Kopija datotek narejena v {dir_original}{Style.RESET_ALL}")
        time.sleep(1)
    else:
        print(f"{Fore.LIGHTRED_EX}Neznan odgovor. Veljavna sta le Y/N.{Style.RESET_ALL}")
        create_backup(image_dir=image_dir)


def ignore_files(dir, files):
    return [f for f in files if os.path.isfile(os.path.join(dir, f)) and not
                                os.path.join(dir, f).lower().endswith(".jpg") and not
                                os.path.join(dir, f).lower().endswith(".png") and not
                                os.path.join(dir, f).lower().endswith(".bmp")]


def resize_image(image: str, max_length: int):
    print(f"{Fore.YELLOW}Slika: {image}{Style.RESET_ALL}")

    img_fsize_before = os.path.getsize(image)

    with Image.open(image) as img:
        if image.lower().endswith(".png") or image.lower().endswith(".bmp"):
            img = img.convert('RGB')  # converts PNG/BMP to JPG
            img_path = image.replace(".png", ".jpg").replace(".PNG", ".jpg").replace(".bmp", ".jpg").replace(".BMP", ".jpg")
            img.save(fp=img_path)
            img_fsize_smt2jpg = os.path.getsize(img_path)
            reduced_size = BtoMB(100 - (img_fsize_smt2jpg / img_fsize_before) * 100)
            print(f"{Fore.GREEN}Pretvorjena iz PNG/BMP v JPG (-{reduced_size}% | {str(BtoMB(img_fsize_before)).replace('.', ',')} "
                  f"MB => {str(BtoMB(img_fsize_smt2jpg)).replace('.', ',')} MB){Style.RESET_ALL}")
            os.remove(image)  # izbriše PNG/BMP sliko
        else:
            img_path = image
            img_fsize_smt2jpg = img_fsize_before

        width = img.width
        height = img.height

        if max(width, height) > max_length:
            if width == height:
                img = img.resize((max_length, max_length))
            else:
                short_side = round((min(width, height)/max(width, height)) * max_length)

                if width > height:
                    img = img.resize((max_length, short_side))
                if height > width:
                    img = img.resize((short_side, max_length))

            img.save(fp=img_path)
            img_fsize_resized = os.path.getsize(img_path)
            reduced_size_total = round(100 - (img_fsize_resized / img_fsize_before) * 100, 2)
            print(f"{Fore.LIGHTGREEN_EX}Zmanjšana z {width}x{height} na {img.width}x{img.height} "
                  f"(sprememba velikosti datoteke: -{reduced_size_total}% | {str(BtoMB(img_fsize_before)).replace('.', ',')} "
                  f"MB => {str(BtoMB(img_fsize_resized)).replace('.', ',')} MB){Style.RESET_ALL}")
            return True, BtoMB(img_fsize_before - img_fsize_smt2jpg), BtoMB(img_fsize_before - img_fsize_resized)

        else:
            print(f"{Fore.LIGHTBLUE_EX}Velikost ustrezna ({width}x{height}) preskakujem...{Style.RESET_ALL}")
            return False, BtoMB(img_fsize_before - img_fsize_smt2jpg), 0


def BtoMB(bytes):
    return round(bytes / 1000000, 2)


def MBtoGBstr(megabytes):
    mb = f"{megabytes:.2f} MB"
    gb = f"{megabytes/1000:.2f} GB"
    return f"{mb.ljust(10, ' ')} |  {gb}"


if __name__ == "__main__":
    init()
    try:
        main()
    except Exception as e:
        print("Unknown error:\n", e)
        print("-"*35)
        print(traceback.format_exc())
        input("Screenshotaj napako in mi pošlji.")
        input()
