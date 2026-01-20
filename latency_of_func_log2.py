import re
import sys

def verify_and_summary_log_file(file_path):
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            lines = f.readlines()
    except FileNotFoundError:
        print(f"File not found: {file_path}")
        return

    # 1. 그래프 이름(functionGraphDesc_name) 추출을 위한 패턴
    graph_name_pattern = re.compile(r"functionGraphDesc_name:\s*(?P<name>.+)")
    
    # 2. 기존 패턴들
    struct_pattern = re.compile(r"(?P<key>[\w]+):\s+!(?P<value>[\w\.]+)")
    pattern = re.compile(r"\[(?P<func>.*?)\] Start: (?P<start>[\d\.]+) ms \| End: (?P<end>[\d\.]+) ms \| Accumulated: (?P<acc>[\d\.]+) ms")
    abs_start_pattern = re.compile(r"^Start:\s+(?P<time>[\d\.]+) ms")
    abs_end_pattern = re.compile(r"^end:\s+(?P<time>[\d\.]+) ms")

    # 모든 그래프 이름을 담을 set
    all_graph_names = set()
    
    func_summary = {}
    pending_stores = []

    abs_start = -1.0
    abs_end = -1.0
    prev_end = -1.0
    first_func_started = False
    total_latency = 1.0 
    res_time = 0.0
    count = 1

    print(f"------------- Analysis for '{file_path}' -------------")

    # 로그 스캔 시작
    for line in lines:
        line = line.strip()
        if not line: continue

        # [추가] functionGraphDesc_name 추출
        if graph_match := graph_name_pattern.search(line):
            all_graph_names.add(graph_match.group('name').strip())
            continue

        if start_match := abs_start_pattern.search(line):
            abs_start = float(start_match.group('time'))
            continue

        if end_match := abs_end_pattern.search(line):
            abs_end = float(end_match.group('time'))
            continue

        if line.startswith("Compressed"):
            m2 = re.search(r"(\d+\.?\d*)(?=\s*ms)", line)
            if m2: total_latency = float(m2.group(1))
            continue

        # 특수 태그 처리 (store가 포함된 경우만)
        if struct_match := struct_pattern.search(line):
            tag_value = struct_match.group('value')
            if 'store' in tag_value.lower():
                pending_stores.append("zl.store_combined")
            continue

        # 메인 함수 로그 처리
        if match := pattern.search(line):
            func = match.group('func')
            start = float(match.group('start'))
            end = float(match.group('end'))
            acc = float(match.group('acc'))

            if not first_func_started and abs_start != -1.0:
                func_summary["INIT_START_TO_FIRST_START"] = [start - abs_start, 1]
                first_func_started = True

            if prev_end != -1.0:
                gap = start - prev_end
                if pending_stores:
                    share = gap / len(pending_stores)
                    for spec in pending_stores:
                        if spec not in func_summary: func_summary[spec] = [0.0, 0]
                        func_summary[spec][0] += share
                        func_summary[spec][1] += 1
                    pending_stores = []
                else:
                    if count > 1: res_time += gap

            if func not in func_summary: func_summary[func] = [0.0, 0]
            func_summary[func][0] = acc 
            func_summary[func][1] += 1
            prev_end = end
            count += 1

    # 결과 출력 전 그래프 이름 목록 먼저 보여주기
    print("\n[Detected Graph Names (Set)]")
    for gname in sorted(list(all_graph_names)):
        print(f" - {gname}")
    print("-" * 60)

    # 마지막 지연 시간 계산
    if abs_end != -1.0 and prev_end != -1.0:
        func_summary["LAST_END_TO_FINAL_END"] = [abs_end - prev_end, 1]

    # 분석 결과 출력
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
    print(f"Actual total_latency:        {total_latency:.4f} ms\n")

if __name__ == "__main__":
    if len(sys.argv) < 2: print("arg error")
    else: verify_and_summary_log_file(sys.argv[1])