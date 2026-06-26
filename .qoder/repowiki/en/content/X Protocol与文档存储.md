
<cite>
**引用文件**
- [plugin/x/](file://plugin/x/)
- [plugin/x/src/](file://plugin/x/src/)
- [plugin/x/protocol/](file://plugin/x/protocol/)
- [plugin/x/client/](file://plugin/x/client/)
- [sql/json_duality_view/](file://sql/json_duality_view/)
</cite>

## 目录
1. [X Plugin 概述](#x-plugin-概述)
2. [架构](#架构)
3. [X Protocol 协议](#x-protocol-协议)
4. [文档存储 API](#文档存储-api)
5. [CRUD 操作](#crud-操作)
6. [管道与异步](#管道与异步)
7. [JSON Duality Views 集成](#json-duality-views-集成)
8. [MySQL Shell 集成](#mysql-shell-集成)

## X Plugin 概述

X Plugin 是 MySQL 的内置插件，通过 X Protocol 协议提供文档存储和 NoSQL 风格的 API 访问。实现在 `plugin/x/` 目录中，包含 58 个源文件。X Plugin 随 MySQL Server 一起安装，默认在端口 33060 上监听。

**Sources** · [plugin/x/](file://plugin/x/)

## 架构

```mermaid
graph TB
    subgraph 客户端
        SHELL[MySQL Shell]
        CONNECTOR_PY[Connector/Python]
        CONNECTOR_J[Connector/J]
        CONNECTOR_N[Connector/Node.js]
        CONNECTOR_CXX[Connector/C++]
    end

    subgraph X Plugin — plugin/x/
        SERVER[X Protocol 服务器]
        PROTO[协议编解码 — protocol/]
        CLIENT_LIB[客户端库 — client/]
        HANDLER[请求处理器 — src/]
    end

    subgraph MySQL Server
        SQL_LAYER[SQL 层]
        INNODB[InnoDB 存储引擎]
    end

    SHELL --> SERVER
    CONNECTOR_PY --> SERVER
    CONNECTOR_J --> SERVER
    CONNECTOR_N --> SERVER
    CONNECTOR_CXX --> SERVER
    SERVER --> PROTO
    PROTO --> HANDLER
    HANDLER --> SQL_LAYER
    SQL_LAYER --> INNODB
```

### 目录结构

| 目录 | 说明 |
|------|------|
| `src/` | 核心服务器代码 — 会话管理、请求处理、认证 |
| `protocol/` | X Protocol 消息编解码 — Protobuf 定义 |
| `client/` | X Protocol 客户端库 — 供连接器和 Shell 使用 |
| `tests/` | 单元测试 |

**Sources** · [plugin/x/src/](file://plugin/x/src/)

## X Protocol 协议

X Protocol 是基于 Protocol Buffers 的二进制协议，设计用于高性能的客户端-服务器通信：

### 协议特点

| 特性 | 说明 |
|------|------|
| **二进制编码** | 基于 Protobuf，比文本协议更高效 |
| **异步支持** | 内置管道（Pipeline）和异步执行 |
| **流式传输** | 支持结果集的流式返回 |
| **多路复用** | 单个连接上支持多个并发请求 |
| **可扩展** | 通过 Notice 消息实现服务器推送 |

### 消息类型

| 消息 | 方向 | 说明 |
|------|------|------|
| `Session.Reset` | C→S | 重置会话状态 |
| `Session.Close` | C→S | 关闭会话 |
| `Sql.StmtExecute` | C→S | 执行 SQL 语句 |
| `Crud.Find` | C→S | 文档查找 |
| `Crud.Insert` | C→S | 文档插入 |
| `Crud.Update` | C→S | 文档更新 |
| `Crud.Delete` | C→S | 文档删除 |
| `Resultset.ColumnMetaData` | S→C | 列元数据 |
| `Resultset.Row` | S→C | 数据行 |
| `Notice` | S→C | 通知消息 |

**Sources** · [plugin/x/protocol/](file://plugin/x/protocol/)

## 文档存储 API

X Protocol 将 MySQL 转变为同时支持关系型和文档模型的数据库：

### 集合（Collection）

集合是文档存储的基本单位，映射到 InnoDB 表的 JSON 列：

```javascript
// MySQL Shell — JavaScript 模式
// 创建 Schema 和 Collection
var db = session.getSchema('mydb');
db.createCollection('users');

// 集合底层是一个包含 JSON 列的 InnoDB 表
// CREATE TABLE `users` (
//   `doc` JSON,
//   `_id` VARBINARY(32) GENERATED ALWAYS AS (...) STORED NOT NULL,
//   PRIMARY KEY (_id)
// )
```

### 文档 ID

每个文档自动分配一个 `_id` 字段（UUID 格式），作为主键：

```javascript
// 插入时自动生成 _id
db.users.add({name: "Alice", age: 30});
// 自动分配: _id: "000062a3-..."

// 手动指定 _id
db.users.add({_id: "user001", name: "Bob", age: 25});
```

**Sources** · [plugin/x/src/](file://plugin/x/src/)

## CRUD 操作

### 插入（Add）

```javascript
// 单个文档
db.users.add({name: "Alice", age: 30, email: "alice@example.com"}).execute();

// 批量插入
db.users.add(
  {name: "Bob", age: 25},
  {name: "Carol", age: 28},
  {name: "Dave", age: 35}
).execute();
```

### 查询（Find）

```javascript
// 基本查询
db.users.find("age > :min_age AND name LIKE :pattern")
  .bind("min_age", 25)
  .bind("pattern", "A%")
  .execute();

// 投影
db.users.find("age > 25", "{name, age}")
  .sort("age DESC")
  .limit(10)
  .execute();
```

### 更新（Modify）

```javascript
// 更新匹配文档
db.users.modify("name = 'Alice'")
  .set("age", 31)
  .set("updated_at", new Date())
  .execute();

// 数组操作
db.users.modify("name = 'Bob'")
  .arrayAppend("tags", "premium")
  .execute();
```

### 删除（Remove）

```javascript
// 删除匹配文档
db.users.remove("age < 18").execute();

// 删除所有文档
db.users.remove("true").execute();
```

**Sources** · [plugin/x/src/](file://plugin/x/src/)

## 管道与异步

X Protocol 原生支持管道操作，减少网络往返：

### 管道模式

```javascript
// 多条语句批量发送
var stmts = [];
for (var i = 0; i < 1000; i++) {
  stmts.push(
    db.users.add({name: "user_" + i}).execute()
  );
}
// 所有语句一起发送到服务器
// 结果异步返回
```

### 异步执行

```javascript
// 异步执行
var promise = db.users.find("age > 25").execute();
promise.then(function(result) {
  // 处理结果
});
```

## JSON Duality Views 集成

X Protocol 与 JSON Relational Duality Views 深度集成：

```javascript
// Duality View 可作为集合访问
var student_dv = session.getDefaultSchema()
  .getCollection("student_dv");

// 通过文档 API 访问关系数据
student_dv.find({_id: 1}).execute();

// 通过文档 API 修改关系数据
student_dv.modify("_id = 1")
  .set("name", "Updated Name")
  .execute();
```

**Sources** · [sql/json_duality_view/](file://sql/json_duality_view/)

## MySQL Shell 集成

MySQL Shell 是 X Protocol 的主要客户端，提供三种语言接口：

| 语言 | 模式 | 示例 |
|------|------|------|
| **JavaScript** | `\js` | `db.users.find()` |
| **Python** | `\py` | `db.users.find()` |
| **SQL** | `\sql` | `SELECT * FROM users` |

### 连接方式

```bash
# X Protocol 连接（端口 33060）
mysqlsh --uri root@localhost:33060

# Classic Protocol 连接（端口 3306）
mysqlsh --uri root@localhost:3306 --mysql

# InnoDB Cluster 连接
mysqlsh --uri root@cluster-host:33060
```

### 管理命令

```javascript
// 查看 Schema 和 Collection
session.getSchemas();
db.getCollections();

// 创建索引（加速文档查询）
db.users.createIndex("age_idx", {
  fields: [{field: "$.age", type: "INT"}]
});

// 查看集合信息
db.users.count();
```

**Sources** · [plugin/x/client/](file://plugin/x/client/)
