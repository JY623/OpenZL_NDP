import re
import sys

def verify_and_summary_log_file(file_path):
    try:
        with open(file_path, 'r') as f:
            lines = f.readlines()
    except FileNotFoundError:
        print(f"File is not found: {file_path}")
        return

    struct_pattern = re.compile(r"(?P<key>[\w]+):\s+!(?P<value>[\w\.]+)")
    pattern = re.compile(r"\[(?P<func>.*?)\] Start: (?P<start>[\d\.]+) ms \| End: (?P<end>[\d\.]+) ms \| Accumulated: (?P<acc>[\d\.]+) ms")

    # 사용자님이 요청하신 시작/끝 마커
    abs_start_pattern = re.compile(r"^Start:\s+(?P<time>[\d\.]+) ms")
    abs_end_pattern = re.compile(r"^end:\s+(?P<time>[\d\.]+) ms")

    graph_tags = set()
    func_summary = {}
    pending_specials = []

    abs_start = -1.0
    abs_end = -1.0
    prev_end = -1.0
    first_func_started = False

    total_latency = 1.0 # Compressed... 라인에서 가져올 값
    res_time = 0.0
    count = 1

    print(f"------------- '{file_path}' -------------")

    for line in lines:
        line = line.strip()
        if not line: continue

        # 1. 절대 시작 시간 (Start: ...)
        start_match = abs_start_pattern.search(line)
        if start_match:
            abs_start = float(start_match.group('time'))
            continue

        # 2. 절대 종료 시간 (end: ...)
        end_match = abs_end_pattern.search(line)
        if end_match:
            abs_end = float(end_match.group('time'))
            continue

        # 3. 전체 레이턴시 정보 (Compressed...)
        if line.startswith("Compressed"):
            m2 = re.search(r"(\d+\.?\d*)(?=\s*ms)", line)
            if m2: total_latency = float(m2.group(1))
            print("-" * 40)
            print(line)
            continue

        # 4. 특수 태그 처리
        struct_match = struct_pattern.search(line)
        if struct_match:
            tag_value = struct_match.group('value')
            graph_tags.add(tag_value)
            pending_specials.append(tag_value)
            continue

        # 5. 메인 함수 로그 처리
        match = pattern.search(line)
        if match:
            func = match.group('func')
            start = float(match.group('start'))
            end = float(match.group('end'))
            acc = float(match.group('acc'))

            # [추가] 시작부터 첫 번째 함수 시작까지의 Latency (S2S)
            if not first_func_started and abs_start != -1.0:
                initial_gap = start - abs_start
                name = "INIT_START_TO_FIRST_START"
                if name not in func_summary: func_summary[name] = [0.0, 0]
                func_summary[name][0] += initial_gap
                func_summary[name][1] += 1
                first_func_started = True

            # 기존 Gap 처리 로직 (Special Tag 배분 포함)
            if prev_end != -1.0:
                gap = start - prev_end
                if pending_specials:
                    share = gap / len(pending_specials)
                    for spec in pending_specials:
                        unified = "zl.store_combined" if 'store' in spec.lower() else spec
                        if unified not in func_summary: func_summary[unified] = [0.0, 0]
                        func_summary[unified][0] += share
                        func_summary[unified][1] += 1
                    pending_specials = []
                else:
                    if count > 1: res_time += gap

            # 일반 함수 통계
            if func not in func_summary: func_summary[func] = [0.0, 0]
            func_summary[func][0] = acc # Accumulated 사용
            func_summary[func][1] += 1

            prev_end = end
            count += 1

    # [추가] 마지막 함수 종료부터 최종 end까지의 Latency (E2E)
    if abs_end != -1.0 and prev_end != -1.0:
        final_gap = abs_end - prev_end
        name = "LAST_END_TO_FINAL_END"
        if name not in func_summary: func_summary[name] = [0.0, 0]
        func_summary[name][0] += final_gap
        func_summary[name][1] += 1

    # --- 기존 결과 출력 구조 유지 ---
    print(f"{'Function name':<45} | {'Total latency (ms)':>20} | {'Count':>10} | {'Ratio':>15}")
    print("-" * 100)

    total_measured_time = 0
    for name, data in sorted(func_summary.items(), key=lambda x: x[1][0], reverse=True):
        ratio = (data[0] / total_latency) if total_latency > 0 else 0
        print(f"{name:<45} | {data[0]:>20.4f} | {data[1]:>10} | {ratio:>15.2%}")
        total_measured_time += data[0]

    print("-" * 100)
    print(f"Total Measured (Sum):        {total_measured_time:.4f} ms")
    print(f"Total Residual (Gap):        {res_time:.4f} ms")
    print(f"Actual total_latency:        {total_latency:.4f} ms")
    print("-" * 40)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("arg error")
    else:
        verify_and_summary_log_file(sys.argv[1])