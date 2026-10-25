# Giao thức v0.1 — guest milestone

## Transport

TCP IPv4. Một frame = `uint32` độ dài **network byte order (big endian)** + đúng số byte JSON UTF-8.
Độ dài không gồm 4 byte tiền tố; 1–16384 byte. Không gửi ký tự NUL hay xuống dòng ngoài payload.
Mỗi JSON phải là object. Sai framing/JSON đóng kết nối; sai nghiệp vụ trả ERROR.
Các Stream tách ranh giới thông điệp và giữ phần chưa đủ byte. Hàng đợi có giới hạn; slow client
làm tràn hàng đợi bị ngắt để không chặn toàn bộ server.

Client gửi `request_id` dạng chuỗi 1–64 ký tự trong mọi request. Server trả cùng ID trong phản hồi trực tiếp.
STATE là sự kiện broadcast nên cả hai client có thể thấy ID của người gửi MOVE, không dùng ID đó để nhận diện người chơi.
LOBBY/INVITE/GAME_OVER là sự kiện chủ động, không có request_id. LIST chỉ phát LOBBY.
Server chưa cache request_id: **không tự động retry MOVE/RESIGN** sau timeout.

## Client → server

```json
{"type":"HELLO","request_id":"1","name":"Loc"}
{"type":"LIST","request_id":"2"}
{"type":"CHALLENGE","request_id":"3","target":"Viet"}
{"type":"ACCEPT","request_id":"4"}
{"type":"DECLINE","request_id":"5"}
{"type":"MOVE","request_id":"6","room_id":1,"uci":"e2e4"}
{"type":"MOVE","request_id":"7","room_id":1,"uci":"e7e8q"}
{"type":"RESIGN","request_id":"8","room_id":1}
{"type":"PING","request_id":"9"}
```

- HELLO bắt buộc trước các lệnh khác; quá 15 giây chưa HELLO thì đóng kết nối.
- Tên guest: 1–24 ký tự ASCII `[A-Za-z0-9_]`, phân biệt hoa thường, không trùng người đang kết nối.
- CHALLENGE: đối thủ phải ready, khác bản thân, cả hai không có lời mời đang chờ. Hết hạn sau 30 giây.
- ACCEPT/DECLINE áp dụng cho lời mời duy nhất của người nhận. Người gửi cầm trắng, người nhận cầm đen.
- `room_id` là ID ván trong database, không phải chỉ số mảng phòng; phải khớp ván của socket hiện tại.
- UCI dùng chữ thường, ví dụ `g1f3`, `e1g1`, `e7e8q`; phong cấp bắt buộc thêm `q/r/b/n`.
- Người chơi được xác định từ socket, không tin tên/color mà client tự truyền.

## Server → client

```json
{"type":"WELCOME","request_id":"1"}
{"type":"LOBBY","players":[{"name":"Loc","status":"ready"},{"name":"Viet","status":"playing"}]}
{"type":"CHALLENGE_SENT","request_id":"3"}
{"type":"INVITE","from":"Loc","expires_in":30}
{"type":"INVITE_CLOSED","reason":"declined"}
{"type":"OK","request_id":"4"}
{"type":"PONG","request_id":"9"}
{"type":"ERROR","request_id":"6","code":"ILLEGAL_MOVE","detail":"Move leaves king in check"}
```

STATE có các trường:

| Trường | Ý nghĩa |
|---|---|
| type | `STATE` |
| room_id | ID ván SQLite |
| ply | Số nửa nước đã đi, bắt đầu 0 |
| white / black | Tên hai người |
| board | Đúng 64 ký tự, từ a8 đến h8, rồi a7… đến h1 |
| fen | FEN đầy đủ gồm lượt, nhập thành, en passant, halfmove, fullmove |
| turn | `white` hoặc `black` |
| check | Bên đến lượt có đang bị chiếu không |
| last_move | UCI của nước vừa đi, rỗng lúc bắt đầu |
| request_id | ID của MOVE/ACCEPT gây ra sự kiện |

`PNBRQK` là quân trắng, `pnbrqk` là quân đen, `.` là ô trống.
Khởi tạo bàn:

```text
rnbqkbnr
pppppppp
........
........
........
........
PPPPPPPP
RNBQKBNR
```

Server lưu thành công trước khi phát STATE. Không đổi bàn khi nhận ERROR.
Khi có nước kết thúc: STATE cuối → GAME_OVER → LOBBY. Client giữ bàn cuối nhưng khóa thao tác.

```json
{"type":"GAME_OVER","room_id":1,"result":"0-1","reason":"checkmate"}
```

`result`: `1-0`, `0-1`, `1/2-1/2`. `reason`: `checkmate`, `stalemate`, `resign`, `disconnect`.
Khi server dừng/khởi động lại, ván dang dở được lưu `aborted`; chưa có cơ chế resume/thông báo phục hồi.

Các mã lỗi: BAD_REQUEST_ID, ALREADY_JOINED, BAD_NAME, NAME_TAKEN, HELLO_REQUIRED, BUSY,
BAD_TARGET, TARGET_BUSY, NO_INVITE, SERVER_FULL, STORAGE_ERROR, NOT_IN_GAME, BAD_ROOM,
NOT_YOUR_TURN, ILLEGAL_MOVE, UNKNOWN_COMMAND. Client phải hiển thị lỗi và cho người dùng tiếp tục thao tác phù hợp.

## Chưa có trong v0.1

REGISTER/LOGIN, Elo, MATCHMAKE, PAUSE/RESUME, DRAW_OFFER/ACCEPT, REMATCH, HISTORY, REPLAY.
Không gửi những lệnh này và không hiển thị chúng như tính năng đã hoạt động.
