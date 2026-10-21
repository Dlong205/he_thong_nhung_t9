# Báo cáo K03 – nguồn LaTeX

> Bản báo cáo/PDF này là bản nháp đo đạc ngày 20/10/2026. Phần mô tả servo ESP32-S3 và một bus I2C ghi lại cấu hình thử nghiệm cũ; cấu hình firmware hiện tại dùng servo PA1 trên STM32, OLED/QMC trên I2C1 PB6/PB7 và MPU trên I2C2 PB10/PB11. Cần cập nhật nội dung và biên dịch lại trước khi nộp.

## Nội dung
- `bao_cao_k03.pdf`: bản đọc và in.
- `bao_cao_k03.tex`: nội dung, định dạng và bố cục.
- `phan_tich_ly_thuyet.tex`, `phan_tich_thiet_ke.tex`, `phan_tich_ket_qua.tex`: các phần phân tích chi tiết được nạp từ tệp chính.
- `anh_san_pham.tex`: các vị trí cần thay bằng ảnh sản phẩm thật trước khi nộp.
- `quy_trinh_kiem_chung.tex`: quy trình bổ sung chuẩn góc, video và các phép đo còn thiếu.
- `thong_tin.tex`: điền tên giảng viên, nhóm và bốn thành viên.
- `figures/`: logo lấy từ mẫu Word, sơ đồ TikZ và đồ thị PDF.
- `data/`: ba tập dữ liệu đo được sử dụng để dựng đồ thị.
- `make_figures.py`: dựng lại đồ thị từ dữ liệu gốc; cần Python 3, matplotlib, numpy.

## Biên dịch
Cài Tectonic hoặc TeX Live có XeLaTeX. Cài font Times New Roman để có diện mạo giống bản PDF đã giao. Nếu không có font này, mã tự dùng Liberation Serif; ngắt dòng có thể thay đổi. Font Microsoft không được đóng gói trong tệp nguồn.

Chạy tại thư mục chứa tệp chính:

```sh
tectonic bao_cao_k03.tex
```

Hoặc chạy XeLaTeX ba lần để cập nhật mục lục và tham chiếu:

```sh
xelatex bao_cao_k03.tex
xelatex bao_cao_k03.tex
xelatex bao_cao_k03.tex
```

Không cần dựng lại đồ thị để biên dịch. Khi muốn tái tạo đồ thị, chạy `python3 make_figures.py` trước khi biên dịch.

## Thông tin cần hoàn tất trước khi nộp
Trang bìa giữ bố cục và tên môn của file mẫu; thông tin giảng viên và bốn thành viên nhóm K03 nằm trong `thong_tin.tex`. Bản hiện tại dài khoảng 55 trang A4, tùy font có sẵn. Sơ đồ nối chân thể hiện kết nối theo mã nguồn, không phải ảnh chụp mạch thực tế. Sáu khung trong `anh_san_pham.tex` dành cho ảnh thật. Khi có ảnh, đặt chúng trong `figures/`, thay từng lệnh `\ChoAnhThucTe{...}{...}` bằng môi trường `figure` có `\includegraphics[width=0.86\textwidth,keepaspectratio]{figures/ten_anh.jpg}` và giữ caption tương ứng. Biên dịch lại để cập nhật danh mục hình.

Báo cáo dùng cả dữ liệu lịch sử tháng 9 và phép đo tích hợp ngày 20/10/2026. File cuối có 1.728 frame hợp lệ, xác nhận nhịp 500 Hz, thời gian xử lý, chuỗi servo và độ nhất quán float/fixed. Giao diện Web Serial đã hỗ trợ packet nhị phân/CRC, đồ thị và mô hình 3D. Báo cáo không tuyên bố sai số dưới 1 độ vì góc servo vẫn là góc lệnh và mốc dây chỉ kiểm tra khả năng quay về. Trước khi nộp cần điền thông tin nhóm, thay các khung bằng ảnh thật và quay video demo.

Bố cục tham chiếu: mẫu Word “Xử lý tiếng vang trong tín hiệu âm thanh”: bìa có khung/logo, Times New Roman 14, khoảng cách dòng, lời cảm ơn, mục lục, danh mục hình/bảng, các chương và tài liệu tham khảo. Hình thuật toán và kết quả được thay bằng nội dung phù hợp K03.
