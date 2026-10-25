# Kiểm thử và nghiệm thu

## Kết quả đã quan sát tại máy phát triển

Môi trường: Windows với GCC MSYS2 UCRT64/SQLite; đã chuẩn bị thêm Ubuntu/WSL với GCC,
GTK 4.22.4, SQLite 3.46.1 và Xvfb để kiểm tra môi trường đích.

| Hạng mục | Kết quả |
|---|---|
| Biên dịch server và test C với -Wall -Wextra -Wpedantic | Đạt trên Windows và Ubuntu |
| Unit tests | PASS 135 kiểm tra |
| Perft vị trí đầu | Depth 1 = 20, depth 2 = 400, depth 3 = 8902 |
| Hai client TCP thật | PASS bằng Python harness, không phải mock server |
| SQLite transaction/rollback | PASS; duplicate ply và ghi vào ván đã kết thúc không làm hỏng dữ liệu |
| Biên dịch/chạy GUI GTK | Đạt trên Ubuntu/WSL; có cảnh báo -Wpedantic trong header GTK 4.22.4 của hệ thống |
| Hai GUI với server thật | Đạt: gọi signal clicked của widget, lời mời, 4 nước tới checkmate, khóa bàn sau kết thúc |
| Ảnh render GTK | Đã xem ảnh hai bên ở build/gui-Loc.png, build/gui-Viet.png |
| Hai máy LAN / demo tương tác thủ công | Chưa xác minh |
| GitHub Actions | Đã tạo workflow; chưa có remote/run để xác minh |

Unit tests bao gồm: lượt, đường đi, vua bị chiếu, chiếu hết, stalemate, nhập thành,
nhập thành qua ô bị tấn công, phong cấp, bắt tốt qua đường, en passant làm hở vua,
không bắt vua, framing chia từng byte/gộp frame/JSON sai/quá giới hạn và SQLite rollback.

Integration test khởi động server tạm, dùng database riêng, không ghi đè `chess.db` của người dùng.
Kiểm tra tên trùng, danh sách online, tự thách đấu, từ chối/nhận lời mời, sai lượt, sai phòng,
nước bất hợp lệ, đồng bộ hai bên, Fool's mate, chịu thua, mất kết nối, frame lỗi và dữ liệu đã lưu.

## Lệnh chạy

```bash
make all
make test
make integration
make gui-test
```

Hoặc:

```text
python scripts/build.py test
python scripts/build.py integration
python scripts/build.py gui
```

## Nghiệm thu thủ công tiếp theo

Smoke test tự động đã chạy qua các callback GTK thật với hai process GUI và một server thật,
không phải dữ liệu giả. Cần tiếp tục kiểm tra các tình huống tương tác, lỗi và môi trường LAN dưới đây.

1. Trên Ubuntu cài các dependency trong README, `make all` phải build được client.
2. Mở server và hai client với tên khác nhau. Cửa sổ không lỗi font quân cờ, không cắt bàn/nút.
3. Thấy cả hai tên trong lobby. Thử từ chối lời mời, thử nhận lời mời và để hết hạn 30 giây.
4. Trắng đi e2e4, đen đi e7e5: hai cửa sổ có cùng bàn, bên đen nhìn xoay bàn.
5. Thử đi khi chưa đến lượt, đi quân xuyên quân cản và để vua bị chiếu: bàn giữ nguyên, có lỗi rõ ràng.
6. Thực hiện ván `f2f3 e7e5 g2g4 d8h4`: cả hai thấy checkmate, kết quả 0-1 và nút đi bị khóa.
7. Thách đấu lại bằng luồng lobby; thử nút Resign có xác nhận, kết quả đúng phía thắng.
8. Đóng một client đang chơi: phía kia được thông báo thắng vì disconnect, lobby dọn tên.
9. Server dừng: client không treo; kết nối lại được khi server mở lại (ván cũ không resume).
10. Thử cửa sổ nhỏ, phóng to, đóng cửa sổ khi đang kết nối; không crash/treo.
11. Chơi thêm các tình huống nhập thành, en passant và phong cấp, gồm chọn rook/bishop/knight.
12. Lặp lại luồng cơ bản trên hai máy trong LAN, ghi hệ điều hành/phiên bản GTK và ảnh chụp demo.

## Kiểm tra database thủ công

Nếu có sqlite3 CLI:

```bash
sqlite3 chess.db "SELECT id,white,black,result,reason FROM games;"
sqlite3 chess.db "SELECT game_id,ply,uci,fen FROM moves ORDER BY game_id,ply;"
```

Các con số test không chứng minh toàn bộ luật cờ/GUI hoàn hảo. Đặc biệt quy tắc hòa còn nằm ngoài mốc này.
