# Phân công mốc 30% và bàn giao tiếp theo

## Cách hiểu tiến độ

Mốc này đặt nền tảng cho một ván cờ giữa hai máy. Con số 30% là ước lượng phạm vi dự án,
không phải số điểm rubric, số dòng code hay 30% hoàn thành của từng chức năng.
Ba người nhận số gói công việc bằng nhau; công sức thực tế cần điều chỉnh sau khi cùng chạy thử.
Mã hiện tại do trợ lý tạo làm nền chung; từng thành viên phải đọc, kiểm thử và giải thích phần mình nhận.

## Khánh — 4 gói

1. **SV01/SV02:** `server/main.c`, `common/net.c/.h`: listen, poll, accept, tối đa 32 kết nối.
2. **SV03/SV04:** `common/protocol.c/.h`, `docs/GIAO_THUC.md`: độ dài 4 byte, JSON, partial I/O, mã lỗi.
3. **SV06/SV07:** `server/main.c`: guest name, ready/playing, mời đấu và kiểm tra người bận.
4. **SV12:** timeout HELLO, lời mời hết hạn, dọn kết nối và phối hợp đóng phòng.

**Phải giải thích:** vì sao một lần recv không tương ứng một request; vì sao send có thể gửi thiếu;
vì sao socket không được chặn vòng lặp; server xử lý người đang chơi thoát như thế nào.

**Tiếp theo:** LG02/LG03/SV05 đăng ký, đăng nhập, hash mật khẩu bằng libsodium, gắn tài khoản vào socket;
UI02/UI03 màn hình tài khoản và hoàn thiện lobby. Không dùng tên guest như một tài khoản thật.

## Việt — 4 gói

1. **LG04:** `server/chess.c/.h`: bàn 64 ô, lượt, quyền nhập thành, ô bắt tốt qua đường và FEN.
2. **LG05:** di chuyển từng quân, chặn đường, không để vua bị chiếu.
3. **LG06:** nhập thành, bắt tốt qua đường, phong cấp, chiếu hết và stalemate; `tests/test_core.c` phần rules/perft.
4. **LG01/LG10:** `server/storage.c/.h`: games/moves, truy vấn tham số, transaction lưu nước và kết quả.

**Phải giải thích:** khác nhau giữa bị chiếu và chiếu hết; vì sao kiểm tra trên bản sao trước khi commit;
quyền nhập thành; vì sao lưu cả UCI và FEN; rollback khi ghi dữ liệu thất bại.

**Tiếp theo:** hoàn thiện các quy tắc hòa; users/ratings, Elo LG08, matchmaking LG09,
API lịch sử LG11 và UI10/UI11. Đọc chuỗi nước đã lưu để làm replay; không cần thiết kế lại dữ liệu từ đầu.

## Lộc — 4 gói

1. **UI01:** `client/main.c`: worker sở hữu socket, hai queue JSON, luồng GTK xử lý widget.
2. **UI05/UI06:** bàn cờ, chọn quân/ô, xoay bàn phía đen, chọn quân phong cấp.
3. **UI04/UI09/UI08 (một phần):** hộp mời, chấp nhận/từ chối, xác nhận chịu thua, hiển thị kết quả.
4. **SV10/SV11 (một phần), UI07/UI12:** `server/match.c`: nhận MOVE, gọi luật của Việt,
   lưu dữ liệu rồi phát STATE; GUI cập nhật theo server. Kiểm thử tích hợp và xác minh GUI trên Ubuntu.

**Phải giải thích:** GUI không tự coi nước đi là hợp lệ; không gọi GTK từ worker;
vì sao nhập thành/bắt tốt qua đường nên cập nhật toàn bàn; cách chọn phong cấp.

**Tiếp theo:** UI08 hiển thị Elo; LG07/SV11/UI09 tạm dừng, tiếp tục, mời hòa, đấu lại theo trạng thái server;
hoàn thiện CSS, xử lý lỗi kết nối, phối hợp UI12. LG12 để sau các mục bắt buộc.

## Quy tắc phối hợp

- `docs/GIAO_THUC.md` là hợp đồng chung. Thay thông điệp phải cập nhật tài liệu và test cùng lúc.
- `server/app.h` là cấu trúc tích hợp chung; không tự đổi mà không trao đổi với hai người còn lại.
- Việt sở hữu trạng thái cờ; Lộc chỉ gọi hàm luật và phát kết quả; Khánh sở hữu kết nối.
- Khánh phụ trách chính `server/main.c`, Việt phụ trách `chess.*` và `storage.*`, Lộc phụ trách `client/main.c` và `server/match.c`.
- Trước khi ghép code: chạy core test và integration test. Thay GUI phải chạy hai cửa sổ trên Ubuntu.
- Mỗi người tạo nhánh theo tính năng khi đưa dự án lên GitHub; không chia ba bản code rời rồi ghép cuối kỳ.

## Mốc kế tiếp đề xuất

| Mốc | Khánh | Việt | Lộc |
|---|---|---|---|
| Khoảng 50% | Tài khoản/session + UI đăng nhập | Quy tắc hòa + truy vấn lịch sử | Tạm dừng/mời hòa/đấu lại |
| Khoảng 75% | Độ ổn định + sự kiện lobby | Elo + ghép trình độ + replay | Hoàn thiện đồng bộ, kết quả/Elo |
| Hoàn thiện | Test mạng và lỗi | Test luật, dữ liệu | E2E, giao diện, demo |

Các mốc chỉ định hướng; việc xong phải có bằng chứng chạy/test, không tự coi là xong vì có file.
