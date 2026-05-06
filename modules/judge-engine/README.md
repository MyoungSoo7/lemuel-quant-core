# judge-engine

codingtest 사이트의 C++ 코드 채점 엔진. 사용자 소스를 받아 컴파일 후 샌드박스에서 실행하고 verdict 반환.

## 현재 상태 (MVP 스켈레톤)

- [x] CLI 인터페이스 (`source.cpp + input.txt + expected.txt`)
- [x] 컴파일 → 실행 → 출력 비교 파이프라인
- [x] verdict 분류 (AC/WA/TLE/MLE/OLE/RE/CE)
- [ ] seccomp-bpf 시스템콜 화이트리스트 (Linux)
- [ ] cgroup v2 메모리/CPU 제한 (Linux)
- [ ] gRPC 서버 모드 (codingtest 백엔드 연동)
- [ ] 다국어 지원 (Python, Java, Go)

## 빌드

```bash
cmake -B build -DLQC_BUILD_JUDGE_ENGINE=ON
cmake --build build --target judge_engine
```

## 사용

```bash
./build/modules/judge-engine/judge_engine \
    examples/hello.cpp examples/hello.in examples/hello.out
```

## 배포 대상

- 르무엘클라우드 (AWS Lightsail, codingtest 사이트와 같은 호스트)
- 격리: seccomp + cgroup v2 (Ubuntu 22.04+)
