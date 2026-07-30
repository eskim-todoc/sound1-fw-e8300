#ifndef __tdc_test_tx_pmic_h__
#define __tdc_test_tx_pmic_h__

// TX PMIC 전압 측정용 임시 테스트 콘솔.
//
// 제품 동작과 무관한 계측 전용 코드다. main 의 func_normal() 부팅 시퀀스
// 직후에서 호출하며, 호출하면 그 자리에서 무한 루프에 들어가 복귀하지
// 않는다. BLE · 터치 · 자극 등 정상 동작이 전부 멈춘다.
//
// 측정이 끝나면 호출 한 줄만 주석 처리하면 된다.
//
// J-Link RTT Viewer 로 조작한다. 레벨 1 단계 = 25mV.
//
//   방향키 (엔터 불필요)  위 +1 · 아래 -1 · 오른쪽 +10 · 왼쪽 -10
//   w / s / a / d         방향키가 뷰어에서 소비될 때 쓰는 대체 키
//   +N / -N / =N (엔터)   증감 · 직접 설정
//   r (엔터)              PMIC 리셋
//   ? (엔터)              현재 레벨 조회

void tdc_test_tx_pmic_console(void);

#endif  // __tdc_test_tx_pmic_h__
