import socket, subprocess, re, qrcode, os #pip3 install qrcode[pil]
from PIL import Image 

def get_ipv4():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except:
        return "127.0.0.1"

def get_ipv6():
    try:
        s = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
        s.connect(("2001:4860:4860::8888", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except:
        pass
    try:
        out = subprocess.check_output(["ip", "-6", "addr"], text=True)
        for scope in ["global", "link"]:
            for line in out.splitlines():
                if "inet6" in line and scope in line:
                    m = re.search(r"inet6\s+([0-9a-f:]+)", line.strip())
                    if m:
                        return m.group(1)
    except:
        pass
    return "::1"

def make_qr(data):
    qr = qrcode.QRCode(box_size=10, border=4)
    qr.add_data(data)
    qr.make(fit=True)
    return qr.make_image().convert("RGB")

ipv4 = get_ipv4()
ipv6 = get_ipv6()

qr4 = make_qr(f"http://{ipv4}")
qr6 = make_qr(f"http://[{ipv6}]")

canvas = Image.new("RGB", (qr4.width + qr6.width, max(qr4.height, qr6.height)), "white")
canvas.paste(qr4, (0, 0))
canvas.paste(qr6, (qr4.width, 0))
canvas.save("qrDRUMserver.png")

print(f"http://{ipv4}")
print(f"http://[{ipv6}]")