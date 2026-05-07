# 사이트 ↔ lemuel-quant-core 연동 가이드

각 사이트가 lemuel-quant-core 모듈에서 흘러나오는 데이터를 어떻게 소비하는지에 대한 청사진. 사이트 코드(crypto-trading, dart-analytics, codingtest 등)는 별도 레포에 있고, 아래는 그쪽에 추가할 connector 예시.

## 1. crypto 사이트 ← market-feed (Redis pub/sub)

**채널 규약**
- `trade.binance.<symbol>` — 트레이드 (JSON)
- `book.binance.<symbol>` — 호가창 (JSON, depth=20)

**Spring Boot Kotlin 예시 (`crypto-trading`)**

```kotlin
// build.gradle.kts
dependencies {
    implementation("org.springframework.boot:spring-boot-starter-data-redis")
    implementation("io.lettuce:lettuce-core")
    implementation("com.fasterxml.jackson.module:jackson-module-kotlin")
}

// application.yml
spring:
  data:
    redis:
      host: 127.0.0.1
      port: 6379

// LqcMarketFeedSubscriber.kt
@Component
class LqcMarketFeedSubscriber(
    private val redisTemplate: ReactiveRedisTemplate<String, String>,
    private val sink: Sinks.Many<TradeEvent> = Sinks.many().multicast().onBackpressureBuffer(),
) {
    @PostConstruct
    fun subscribe() {
        redisTemplate.listenToPattern("trade.binance.*")
            .map { msg -> jacksonObjectMapper().readValue<TradeEvent>(msg.message) }
            .doOnNext(sink::tryEmitNext)
            .subscribe()
    }
    fun stream(): Flux<TradeEvent> = sink.asFlux()
}

data class TradeEvent(
    val symbol: String,
    val price: Double,
    val qty: Double,
    val side: Int,
    @JsonProperty("ex_ts") val exchangeTsNs: Long,
    @JsonProperty("local_ts") val localTsNs: Long,
)

// LiveTradeController.kt — SSE 로 프런트에 push
@RestController
class LiveTradeController(private val sub: LqcMarketFeedSubscriber) {
    @GetMapping("/api/live/{symbol}", produces = [MediaType.TEXT_EVENT_STREAM_VALUE])
    fun live(@PathVariable symbol: String): Flux<TradeEvent> =
        sub.stream().filter { it.symbol == symbol }
}
```

## 2. dart 사이트 ← dart-crawler (PostgreSQL)

**테이블** (dart-crawler 기동시 자동 생성)

```sql
CREATE TABLE dart_disclosure (
    rcept_no  TEXT PRIMARY KEY,
    corp_code TEXT, corp_name TEXT, stock_code TEXT,
    corp_cls  TEXT, report_nm TEXT, flr_nm TEXT,
    rcept_dt  TEXT, rm TEXT,
    inserted_at TIMESTAMPTZ DEFAULT now()
);
CREATE INDEX idx_dart_disclosure_stock ON dart_disclosure(stock_code, rcept_dt DESC);
```

**Spring Data 예시 (`dart-analytics`)**

```kotlin
// build.gradle.kts
dependencies {
    implementation("org.springframework.boot:spring-boot-starter-data-jpa")
    runtimeOnly("org.postgresql:postgresql")
}

// application.yml
spring:
  datasource:
    url: jdbc:postgresql://127.0.0.1:5433/lqc
    username: lqc
    password: ${LQC_PG_PW}

@Entity @Table(name = "dart_disclosure")
data class Disclosure(
    @Id val rceptNo: String,
    val corpCode: String, val corpName: String, val stockCode: String,
    val corpCls: String, val reportNm: String, val flrNm: String,
    val rceptDt: String, val rm: String,
    val insertedAt: OffsetDateTime,
)

interface DisclosureRepository : JpaRepository<Disclosure, String> {
    fun findTop50ByStockCodeOrderByRceptDtDesc(stockCode: String): List<Disclosure>
    fun findTop100ByOrderByRceptDtDesc(): List<Disclosure>
}

@RestController
class DisclosureController(private val repo: DisclosureRepository) {
    @GetMapping("/api/disclosures/recent")     fun recent() = repo.findTop100ByOrderByRceptDtDesc()
    @GetMapping("/api/disclosures/{stock}")    fun byStock(@PathVariable stock: String) =
        repo.findTop50ByStockCodeOrderByRceptDtDesc(stock)
}
```

## 3. codingtest 사이트 ← judge-engine (gRPC)

**프로토콜**: `lqc.judge.v1.Judge`. `.proto` 는 `modules/judge-engine/grpc/judge.proto`.

**Spring Boot gRPC 클라이언트 예시 (`codingtest`)**

```kotlin
// build.gradle.kts
plugins { id("com.google.protobuf") version "0.9.4" }
dependencies {
    implementation("net.devh:grpc-client-spring-boot-starter:3.1.0.RELEASE")
    implementation("io.grpc:grpc-protobuf:1.66.0")
    implementation("io.grpc:grpc-stub:1.66.0")
}
protobuf {
    protoc { artifact = "com.google.protobuf:protoc:3.25.3" }
    plugins { id("grpc") { artifact = "io.grpc:protoc-gen-grpc-java:1.66.0" } }
    generateProtoTasks { all().forEach { it.plugins { id("grpc") } } }
}
// src/main/proto/judge.proto 에 lemuel-quant-core 의 proto 복사 또는 git submodule

// application.yml
grpc:
  client:
    judge-engine:
      address: static://43.201.110.54:50051   # 르무엘클라우드
      negotiationType: PLAINTEXT

// JudgeService.kt
@Service
class JudgeService(
    @GrpcClient("judge-engine") private val judge: JudgeGrpc.JudgeBlockingStub,
) {
    fun submit(source: String, cases: List<Pair<String, String>>): SubmitResponse {
        val req = SubmitRequest.newBuilder()
            .setLanguage("cpp")
            .setSource(ByteString.copyFromUtf8(source))
            .setLimits(Limits.newBuilder().setWallTimeMs(2000).setCpuTimeMs(1000)
                        .setMemoryBytes(256L * 1024 * 1024).setOutputBytes(64 * 1024))
            .addAllCases(cases.map { (i, e) ->
                TestCase.newBuilder().setInput(ByteString.copyFromUtf8(i))
                    .setExpectedOutput(ByteString.copyFromUtf8(e)).build()
            })
            .build()
        return judge.submit(req)
    }
}
```

## 4. news 사이트 ← news-pipeline

현재 news-pipeline 은 stdout 으로 종목별 점수를 찍고 있고, news 사이트와 연결하려면 score 를 Redis 채널 `news.score.<ticker>` 으로 publish 하는 코드 추가가 필요. (news-pipeline 의 main.cpp 수정 후 재배포)

## 5. data 사이트 ← data-warehouse (R2 Parquet)

R2 에 올라간 `snapshots/rollup-YYYYMMDD-HHMMSS.parquet` 를 시각화. 인증 정보를 사이트 백엔드에 넣기 부담스러우면, data-warehouse 에 작은 REST 게이트웨이를 붙이거나, R2 의 public bucket 으로 전환하여 사이트 프런트에서 직접 읽도록 함.

## 통합 우선순위 (제안)

1. **crypto** — 가장 즉각적, 실시간 효과 큼. 위 Kotlin 코드만 붙이면 SSE 라이브 차트 동작.
2. **dart** — DB 조회만 추가하면 됨. 기존 사이트가 JPA 쓰면 1시간.
3. **codingtest** — gRPC 클라이언트 셋업이 가장 무거움. proto 빌드 통합 필요.
4. **news** — news-pipeline 에 publish 코드 추가 후 사이트가 구독.
5. **data** — data-warehouse R2 데이터 읽기 패턴은 백테스터(`python/backtester`) 가 이미 있음. 사이트는 그 위에 올림.
