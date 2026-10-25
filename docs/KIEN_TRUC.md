# Kiến trúc và ranh giới module

```text
GTK main thread                   Client network worker
  widgets, clicks  --outgoing-->    TCP socket + Stream
  render board     <--incoming--   JSON server events
                                         |
                                    TCP framing
                                         |
server/main.c: poll → Stream → dispatcher / lobby / invitation
                                         |
server/match.c: MOVE → chess_move(copy) → storage_move(transaction)
                                         |
                       commit OK → replace room state → STATE to both clients
```

## Lý do tách lớp

- `common/net.*`: khác biệt POSIX/Windows nằm ở đây. Ubuntu dùng poll(), Windows dùng WSAPoll().
- `common/protocol.*`: chỉ làm framing/JSON và hàng đợi gửi; không biết luật cờ.
- `server/main.c`: một luồng sở hữu clients/rooms, không cần khóa liên luồng cho trạng thái server.
- `server/match.c`: lớp tích hợp mạng với luật và lưu trữ; kiểm tra người chơi/lượt/room_id.
- `server/chess.*`: không phụ thuộc socket, GTK, database; có thể kiểm thử thuần C.
- `server/storage.*`: SQLite truy vấn tham số; transaction đảm bảo nước đi và kết quả cập nhật cùng nhau.
- `client/main.c`: worker không gọi GTK; JSON đi qua GAsyncQueue, GTK lấy tối đa 32 sự kiện mỗi tick 30ms.

## Trạng thái

Client: chưa kết nối → đang kết nối → guest/lobby → có lời mời → trong trận → lobby.
Mất kết nối quay về trạng thái chưa kết nối; không tự nối lại hoặc tự gửi lại nước đi.

Server không lấy bàn, lượt hay kết quả do client khai báo làm dữ liệu chính thức.
Nước đi được thử trên bản sao; sai luật không thay đổi phòng. Ghi SQLite thất bại không broadcast nước đi đó.

`games` lưu tên guest và FEN ban đầu/hiện tại, kết quả/lý do/thời gian.
`moves` có khóa `(game_id, ply)` và lưu UCI + FEN sau từng nước.
Khi bổ sung tài khoản sẽ thêm users/ratings và liên kết ID tài khoản; tên guest hiện chưa phải danh tính xác thực.

## Những giới hạn cần nhớ

- Luật hòa chưa đầy đủ; không tuyên bố đã hoàn thiện toàn bộ luật thi đấu FIDE.
- Chưa có đồng hồ, pause/draw/rematch, tài khoản, TLS, Elo, API lịch sử, replay GUI.
- SQLite và xử lý luật chạy trong event loop, phù hợp quy mô lớp học nhỏ; chưa phải thiết kế chịu tải lớn.
- Không giữ trạng thái RAM qua restart; đánh dấu ván dang dở là aborted để tránh treo dữ liệu.
- GUI đã qua smoke test hai cửa sổ trên Ubuntu/WSL; workflow CI có cùng test và xuất ảnh,
  vẫn cần nghiệm thu thao tác thủ công và thử hai máy LAN.
