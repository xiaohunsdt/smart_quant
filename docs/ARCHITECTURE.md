# 量化交易框架架构图

## 整体架构分层图

```mermaid
graph TB
    subgraph "接口层 Interface Layer"
        API[ApiServer<br/>REST/gRPC接口]
    end
    
    subgraph "策略层 Strategy Layer"
        StrategyBase[StrategyBase<br/>策略基类]
        StrategyFactory[StrategyFactory<br/>策略工厂]
        StrategyScheduler[StrategyScheduler<br/>策略调度器]
        SignalGenerator[SignalGenerator<br/>信号生成器]
        StrategyConfig[StrategyConfigurator<br/>策略配置器]
        Strategies[策略实现<br/>SampleStrategy等]
    end
    
    subgraph "交易层 Trading Layer"
        TradingInterface[ITradingInterface<br/>交易接口]
        OrderManager[OrderManager<br/>订单管理]
        PositionManager[PositionManager<br/>持仓管理]
        AccountManager[AccountManager<br/>账户管理]
        OrderGenerator[OrderGenerator<br/>订单生成]
        ReportHandler[ReportHandler<br/>回报处理]
        TradeImpl[交易接口实现<br/>DummyTradingInterface等]
    end
    
    subgraph "风控层 Risk Control Layer"
        RiskEngine[RiskEngine<br/>风控引擎]
        RiskRules[风控规则<br/>PositionLimitRule等]
        RiskMetrics[RiskMetrics<br/>风险指标]
        CircuitBreaker[CircuitBreaker<br/>熔断器]
        ComplianceChecker[ComplianceChecker<br/>合规检查]
    end
    
    subgraph "数据层 Data Layer"
        DataSource[IDataSource<br/>数据源接口]
        DataParser[DataParser<br/>数据解析]
        DataDistributor[DataDistributor<br/>数据分发]
        MarketCache[MarketCache<br/>行情缓存]
        HistoricalLoader[HistoricalLoader<br/>历史数据加载]
        SourceImpl[数据源实现<br/>BinanceDataSource<br/>DummyDataSource等]
    end
    
    subgraph "基础设施层 Infrastructure Layer"
        Logger[Logger<br/>日志系统]
        Config[ConfigManager<br/>配置管理]
        ThreadPool[ThreadPool<br/>线程池]
        HttpClient[HttpClient<br/>HTTP客户端]
    end
    
    subgraph "核心类型 Core Types"
        Types[Types<br/>TickData<br/>OrderData<br/>Signal<br/>AccountData等]
    end
    
    %% 数据流
    SourceImpl --> DataSource
    DataSource --> DataParser
    DataParser --> DataDistributor
    DataDistributor --> MarketCache
    DataDistributor --> StrategyScheduler
    MarketCache --> Strategies
    
    %% 策略流
    Strategies --> SignalGenerator
    SignalGenerator --> ComplianceChecker
    ComplianceChecker --> RiskEngine
    RiskEngine --> AccountManager
    RiskEngine --> OrderGenerator
    
    %% 交易流
    OrderGenerator --> TradingInterface
    TradingInterface --> TradeImpl
    TradeImpl --> ReportHandler
    ReportHandler --> OrderManager
    ReportHandler --> PositionManager
    TradeImpl --> AccountManager
    
    %% 风控流
    RiskEngine --> RiskRules
    RiskEngine --> RiskMetrics
    RiskEngine --> CircuitBreaker
    AccountManager --> RiskEngine
    
    %% 基础设施依赖
    Logger -.-> Strategies
    Logger -.-> TradingInterface
    Logger -.-> RiskEngine
    Config -.-> StrategyScheduler
    Config -.-> RiskEngine
    ThreadPool -.-> StrategyScheduler
    HttpClient -.-> SourceImpl
    
    %% 接口层
    API -.-> AccountManager
    API -.-> OrderManager
    API -.-> RiskEngine
    
    style API fill:#e1f5ff
    style Strategies fill:#fff4e1
    style TradeImpl fill:#fff4e1
    style SourceImpl fill:#fff4e1
    style RiskEngine fill:#ffe1e1
    style AccountManager fill:#e1ffe1
```

## 数据流程图

```mermaid
sequenceDiagram
    participant DS as 数据源<br/>DataSource
    participant DP as 数据解析<br/>DataParser
    participant DD as 数据分发<br/>DataDistributor
    participant MC as 行情缓存<br/>MarketCache
    participant SS as 策略调度<br/>StrategyScheduler
    participant ST as 策略<br/>Strategy
    participant SG as 信号生成<br/>SignalGenerator
    participant CC as 合规检查<br/>ComplianceChecker
    participant RE as 风控引擎<br/>RiskEngine
    participant AM as 账户管理<br/>AccountManager
    participant OG as 订单生成<br/>OrderGenerator
    participant TI as 交易接口<br/>TradingInterface
    participant OM as 订单管理<br/>OrderManager
    participant PM as 持仓管理<br/>PositionManager
    
    DS->>DP: 原始数据
    DP->>DD: TickData
    DD->>MC: 缓存行情
    DD->>SS: 推送行情
    SS->>ST: on_tick()
    ST->>SG: emit_signal()
    SG->>CC: 标准化信号
    CC->>RE: 合规检查通过
    RE->>AM: 查询账户资金
    AM-->>RE: 返回可用资金
    RE->>RE: 资产校验
    RE->>OG: 风控通过
    OG->>TI: 生成订单
    TI->>OM: 订单回报
    TI->>PM: 成交回报
    TI->>AM: 账户信息更新
```

## 模块依赖关系图

```mermaid
graph LR
    subgraph "框架核心 qf_framework"
        direction TB
        Infra[基础设施层]
        Data[数据层]
        Strategy[策略层]
        Trading[交易层]
        Risk[风控层]
        Interface[接口层]
    end
    
    subgraph "独立模块"
        Sources[数据源实现<br/>sources]
        Strategies[策略实现<br/>strategies]
        Trade[交易接口实现<br/>trade]
    end
    
    subgraph "应用层"
        Main[main.cpp]
    end
    
    Infra --> Data
    Infra --> Strategy
    Infra --> Trading
    Infra --> Risk
    Infra --> Interface
    
    Data --> Strategy
    Strategy --> Trading
    Trading --> Risk
    Risk --> Trading
    
    Sources --> Data
    Strategies --> Strategy
    Trade --> Trading
    
    Main --> Sources
    Main --> Strategies
    Main --> Trade
    Main --> Infra
    Main --> Data
    Main --> Strategy
    Main --> Trading
    Main --> Risk
    
    style Infra fill:#e1f5ff
    style Data fill:#fff4e1
    style Strategy fill:#fff4e1
    style Trading fill:#fff4e1
    style Risk fill:#ffe1e1
    style Sources fill:#f0f0f0
    style Strategies fill:#f0f0f0
    style Trade fill:#f0f0f0
```

## 目录结构图

```
quant_framework/
├── include/
│   ├── qf/                    # 框架核心头文件
│   │   ├── core/              # 核心类型定义
│   │   │   └── types.hpp      # TickData, OrderData, Signal, AccountData等
│   │   ├── infrastructure/    # 基础设施层
│   │   │   ├── logger.hpp     # 日志系统
│   │   │   ├── config.hpp     # 配置管理
│   │   │   └── thread_pool.hpp # 线程池
│   │   ├── common/            # 通用组件
│   │   │   └── http_client.hpp # HTTP客户端
│   │   ├── data/              # 数据层
│   │   │   ├── data_source.hpp
│   │   │   ├── data_parser.hpp
│   │   │   ├── data_distributor.hpp
│   │   │   ├── market_cache.hpp
│   │   │   └── historical_loader.hpp
│   │   ├── strategy/          # 策略层
│   │   │   ├── strategy_base.hpp
│   │   │   ├── strategy_factory.hpp
│   │   │   ├── strategy_scheduler.hpp
│   │   │   ├── signal_generator.hpp
│   │   │   └── strategy_configurator.hpp
│   │   ├── trading/           # 交易层
│   │   │   ├── trading_interface.hpp
│   │   │   ├── order_manager.hpp
│   │   │   ├── position_manager.hpp
│   │   │   ├── account_manager.hpp
│   │   │   ├── order_generator.hpp
│   │   │   └── report_handler.hpp
│   │   ├── risk/              # 风控层
│   │   │   ├── risk_engine.hpp
│   │   │   ├── risk_metrics.hpp
│   │   │   ├── circuit_breaker.hpp
│   │   │   └── compliance_checker.hpp
│   │   └── interface/         # 接口层
│   │       └── api_server.hpp
│   ├── sources/               # 数据源实现（独立）
│   │   ├── dummy_data_source.hpp
│   │   └── binance_data_source.hpp
│   ├── strategies/            # 策略实现（独立）
│   │   └── sample_strategy.hpp
│   └── trade/                 # 交易接口实现（独立）
│       └── dummy_trading_interface.hpp
├── src/
│   ├── qf/                    # 框架核心实现
│   │   └── [对应include/qf的结构]
│   ├── sources/               # 数据源实现
│   ├── strategies/            # 策略实现
│   └── trade/                 # 交易接口实现
├── config/
│   └── config.yaml            # 配置文件
└── src/
    └── main.cpp               # 主程序入口
```

## 关键设计模式

1. **适配器模式**：`IDataSource`、`ITradingInterface` 提供统一接口，不同实现通过适配器接入
2. **观察者模式**：`DataDistributor` 支持订阅/发布行情数据
3. **工厂模式**：`StrategyFactory` 负责策略实例创建
4. **策略模式**：`RiskRule` 接口支持多种风控规则
5. **单例模式**：`Logger`、`StrategyFactory` 使用单例

## 数据流向说明

1. **行情数据流**：数据源 → 解析器 → 分发器 → 缓存/策略
2. **交易信号流**：策略 → 信号生成 → 合规检查 → 风控校验 → 订单生成 → 交易接口
3. **回报数据流**：交易接口 → 回报处理 → 订单管理/持仓管理/账户管理
4. **账户同步流**：交易接口 → 账户管理 → 风控引擎（用于资产校验）

