# Chess Network Lab — mốc nền tảng khoảng 30%

Ứng dụng cờ vua trực tuyến cho bài tập lớn Lập trình mạng, nhóm **Khánh – Việt – Lộc**.
Mã ứng dụng viết bằng **C**; client **GTK 4**, server **TCP/POSIX Socket**, dữ liệu **SQLite**.

**Đây là mốc nền tảng có thể tích hợp, chưa phải 30% chức năng đã nghiệm thu đầy đủ.**
Đã có luồng guest → lobby → thách đấu → hai người đi cờ → kết thúc → lưu dữ liệu.
Chưa có tài khoản/mật khẩu, Elo, ghép trận theo trình độ và giao diện replay.

## Chạy trên Ubuntu (môi trường đích)

```bash
sudo apt update
sudo apt install build-essential pkg-config libgtk-4-dev libsqlite3-dev python3 fonts-dejavu-core
make all
make test
make integration
```

Mở ba terminal trong thư mục dự án:

```bash
# Terminal 1: mặc định chỉ lắng nghe máy hiện tại
./build/chess-server 127.0.0.1 5555 chess.db
```

```bash
# Terminal 2: tên Loc, bấm Connect as guest
./build/chess-client
```

```bash
# Terminal 3: đổi tên thành Viet hoặc Khanh, rồi Connect as guest
./build/chess-client
```

Nhập đúng tên đối thủ ở ô bên phải → **Challenge** → phía kia **Accept**.
Người gửi lời mời cầm trắng. Bấm quân rồi bấm ô đích; server xác nhận mới cập nhật bàn.
Chọn quân phong cấp ở danh sách **Promote to** trước khi đi tốt tới hàng cuối.
Người cầm đen nhìn bàn xoay 180°. Rê chuột lên ô để xem tọa độ.

Chơi trong LAN: chạy server với `0.0.0.0`, nhập **IPv4 LAN thật của máy server** trên hai client.
`0.0.0.0` là địa chỉ bind, không dùng làm địa chỉ kết nối client.
Mở cổng TCP đã chọn trong tường lửa nếu cần. Bản này dành cho thử nghiệm trong lớp/LAN,
chưa có TLS hay xác thực tài khoản để triển khai công khai.

## Máy Windows hiện tại

Trong lúc phát triển đã phát hiện GNU Make tại máy này lỗi với đường dẫn có dấu.
Đã cung cấp script build thay thế, không cần đổi tên thư mục:

```powershell
python scripts/build.py test
python scripts/build.py integration
```

GCC, `pkg-config`, SQLite của MSYS2 UCRT64 phải có trong PATH. Bản server/test có lớp tương thích
Winsock để kiểm thử trên Windows; mã Linux vẫn sử dụng socket và `poll()` của POSIX.
Nếu đã có thư viện phát triển GTK 4 cùng toolchain:

```powershell
python scripts/build.py gui
./build/chess-server.exe 127.0.0.1 5555 chess.db
./build/chess-client.exe
```

**Đã cài dependency và xác minh thêm trong Ubuntu/WSL của máy này:** build được server/client GTK,
135 kiểm tra C, kiểm thử TCP và hai cửa sổ GTK chơi một ván tới chiếu hết đều chạy đạt.
Đã kiểm tra ảnh render tại `build/gui-Loc.png`, `build/gui-Viet.png`.
GTK chưa được cài cho toolchain Windows bản địa; hãy chạy GUI qua Ubuntu/WSL hoặc máy Ubuntu.
Workflow GitHub Actions đã được chuẩn bị; chưa chạy trên GitHub vì chưa có remote.

Từ PowerShell trong thư mục này, có thể build qua Ubuntu đã được chuẩn bị:

```powershell
wsl -d Ubuntu -- make all
wsl -d Ubuntu -- ./build/chess-server 127.0.0.1 5555 chess.db
# Terminal khác, cần WSLg/X server để hiện cửa sổ
wsl -d Ubuntu -- ./build/chess-client
```

Kiểm thử GTK không cần màn hình thật:

```bash
sudo apt install xvfb xauth
make gui-test
```

## Phạm vi đã triển khai

| Nhóm chức năng | Hiện có | Giới hạn |
|---|---|---|
| TCP | Nonblocking I/O, nhiều client, hàng đợi gửi, xử lý chia/gộp thông điệp | Tối đa 32 client, IPv4 số |
| Lobby | Tên khách không trùng, danh sách ready/playing | Không phải đăng ký/đăng nhập |
| Thách đấu | Mời, nhận, từ chối, hết hạn 30 giây, chặn tự mời/người bận | Mỗi người một lời mời đang chờ |
| Phòng | Tối đa 16 phòng 1–1, room_id, lượt trắng/đen | Không nối lại ván sau mất mạng |
| Luật nước đi | Các quân, cản đường, chiếu/an toàn vua, nhập thành, bắt tốt qua đường, phong cấp | Chưa hoàn thiện toàn bộ quy tắc hòa |
| Kết quả | Chiếu hết, hết nước hợp lệ không bị chiếu (stalemate), chịu thua, ngắt kết nối | Chưa hòa do lặp lại, số nước, thiếu lực lượng; chưa có mời hòa |
| Lưu trữ | Bàn đầu, bàn hiện tại, từng nước UCI và FEN, kết quả, thời gian | Chưa users/ratings và API đọc lịch sử/replay |
| GTK client | Kết nối, lobby, lời mời, bàn cờ, gửi nước, cập nhật đối thủ, chọn phong cấp, chịu thua, kết quả | Đã build/chạy trên Ubuntu/WSL qua màn hình ảo; cần demo tương tác/LAN |

Luật di chuyển được kiểm tra trên server; client không tự kết luận nước hợp lệ.
Ván kết thúc khi mất kết nối được tính thua cho bên rời đi ở mốc này, chưa cập nhật Elo.
Server khởi động lại đánh dấu các ván còn dang dở là `aborted`.

## Bàn giao cân bằng

Mỗi người có **4 gói công việc nền tảng**. Đây là phân chia trách nhiệm và khối lượng tương đối,
không phải xác nhận đã hoàn thành toàn bộ các mã trong bảng kế hoạch gốc.

| Khánh — mạng và lobby | Việt — luật và dữ liệu | Lộc — GUI và đồng bộ |
|---|---|---|
| TCP listener + event loop | Mô hình bàn + lượt | Khung client GTK + luồng mạng |
| Framing JSON + partial send/recv | Nước đi hợp lệ + an toàn vua | Bàn 8×8 + thao tác + phong cấp |
| Guest/lobby + thách đấu | Luật đặc biệt + mate/stalemate | Lời mời + phản hồi + chịu thua |
| Dọn kết nối/phòng + log | SQLite + giao dịch lưu nước | Phát STATE/kết quả + đồng bộ GUI |

Đọc [phân công và việc tiếp theo](docs/PHAN_CONG.md), [giao thức](docs/GIAO_THUC.md),
[kiến trúc](docs/KIEN_TRUC.md), [kịch bản nghiệm thu](docs/KIEM_THU.md).

## Cấu trúc

```text
common/       Socket và framing dùng chung
server/       Server, điều phối trận, luật cờ, SQLite
client/       GUI GTK 4
vendor/       cJSON 1.7.18 và giấy phép MIT gốc
tests/        Kiểm thử C và kiểm thử TCP hai client bằng Python
scripts/      Build bằng Python khi Make không dùng được
docs/         Phân công, giao thức, kiến trúc, nghiệm thu
.github/      Workflow build/test trên Ubuntu
```

Python chỉ là công cụ build/kiểm thử; không thay thế phần ứng dụng C.
cJSON được lấy từ kho chính thức `DaveGamble/cJSON`, tag `v1.7.18`;
giữ nguyên giấy phép tại `vendor/cJSON.LICENSE`. Quân cờ hiện dùng ký tự Unicode từ font hệ thống.
