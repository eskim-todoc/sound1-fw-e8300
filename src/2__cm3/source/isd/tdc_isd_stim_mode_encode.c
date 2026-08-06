#include <tdc_isd_stim_mode_encode.h>

/*
  이 파일은 순수 함수만 갖는다.

  공유메모리 · I2C · FPGA 어느 것에도 닿지 않아야 호스트 테스트가 isd/ 의 나머지를
  링크하지 않고 단독으로 검증할 수 있다. 상태를 읽거나 쓰는 코드를 여기 넣지 말 것.
*/

int tdc_isd_stim_mode_reference_bits(EN___STIMULATION_MODE mode)
{
    int bits = en__referenceNA;

    switch (mode)
    {
        case en__monopolr_body:
        {
            bits = en__monopolr_body;
            break;
        }

        case en__monopolr_rod:
        {
            bits = en__monopolr_rod;
            break;
        }

        case en__monopolr_BothRodBody:
        {
            bits = en__monopolr_BothRodBody;
            break;
        }

        default:
        {
            // 바이폴라 · 공통접지 · 동시자극. 기준전극을 쓰지 않으므로 접지를 끊는다.
            bits = en__referenceNA;
            break;
        }
    }

    return bits;
}

int tdc_isd_stim_mode_output_bits(EN___STIMULATION_MODE mode)
{
    int bits = 0;

    switch (mode)
    {
        case en__monopolr_body:
        case en__monopolr_rod:
        case en__monopolr_BothRodBody:
        {
            bits = 0;  // monopolar
            break;
        }

        case en__bipolar:
        {
            bits = 1;
            break;
        }

        case en__commonground:
        {
            bits = 2;
            break;
        }

        case en__semi_simultaneously:
        {
            bits = 3;
            break;
        }

        default:
        {
            /* 원본 5쌍은 default 가 없어, 범위 밖 값이 오면 아무것도 OR 하지 않고
             * 하위 2비트가 0으로 남아 모노폴라처럼 동작했다. 그 동작을 그대로 보존한다.
             * 여기서 값을 바꾸면 순수 통합이 아니라 동작 변경이 된다.
             *
             * en__referenceNA(0) 이 이 경로에 도달 가능한지는 미조사다
             * (docs/tasks/isd/20260806_0834_isd-대형함수-분해-분석/분석.md §3.3). */
            bits = 0;
            break;
        }
    }

    return bits;
}
