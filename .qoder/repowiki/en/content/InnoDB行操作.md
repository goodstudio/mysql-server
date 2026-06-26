
<cite>
**引用文件**
- [storage/innobase/row/row0sel.cc](file://storage/innobase/row/row0sel.cc)
- [storage/innobase/row/row0ins.cc](file://storage/innobase/row/row0ins.cc)
- [storage/innobase/row/row0upd.cc](file://storage/innobase/row/row0upd.cc)
- [storage/innobase/row/row0mysql.cc](file://storage/innobase/row/row0mysql.cc)
- [storage/innobase/row/row0import.cc](file://storage/innobase/row/row0import.cc)
- [storage/innobase/row/row0row.cc](file://storage/innobase/row/row0row.cc)
- [storage/innobase/row/row0vers.cc](file://storage/innobase/row/row0vers.cc)
- [storage/innobase/row/row0log.cc](file://storage/innobase/row/row0log.cc)
</cite>

## 目录
1. [行操作概述](#行操作概述)
2. [行查询（SELECT）](#行查询select)
3. [行插入（INSERT）](#行插入insert)
4. [行更新（UPDATE/DELETE）](#行更新updatedelete)
5. [MySQL 接口层](#mysql-接口层)
6. [表导入导出](#表导入导出)
7. [MVCC 版本链](#mvcc-版本链)
8. [在线 DDL 日志](#在线-ddl-日志)

## 行操作概述

InnoDB 行操作子系统位于 `storage/innobase/row/`，包含 17 个文件，总计约 1.1MB：

| 文件 | 大小 | 说明 |
|------|------|------|
| `row0sel.cc` | ~214KB | SELECT — 行查询（最大的行操作文件） |
| `row0import.cc` | ~154KB | 表空间导入/导出 |
| `row0mysql.cc` | ~147KB | MySQL-InnoDB 接口层 |
| `row0log.cc` | ~127KB | 在线 DDL 临时日志 |
| `row0ins.cc` | ~120KB | INSERT — 行插入 |
| `row0upd.cc` | ~109KB | UPDATE — 行更新 |
| `row0vers.cc` | ~56KB | MVCC 版本链遍历 |
| `row0row.cc` | ~44KB | 行通用操作 |

**Sources** · [storage/innobase/row/](file://storage/innobase/row/)

## 行查询（SELECT）

`row0sel.cc`（~214KB）是 InnoDB 中最大的行操作文件，实现行数据的检索：

### 查询流程

```mermaid
graph TB
    SQL_SELECT[SQL SELECT]
    SQL_SELECT --> HA_READ[handler::ha_read_row]
    HA_READ --> ROW_SEARCH[row_search_mvcc — row0sel.cc]
    ROW_SEARCH --> BTR_OPEN[打开 B-Tree 游标]
    BTR_OPEN --> SEARCH[B-Tree 搜索定位]
    SEARCH --> READ_RECORD[读取记录]
    READ_RECORD --> MVCC_CHECK{MVCC 可见?}
    MVCC_CHECK --> |是| RETURN[返回行]
    MVCC_CHECK --> |否| UNDO[沿 undo 链查找可见版本]
    UNDO --> RETURN
```

### 查询模式

| 模式 | 说明 |
|------|------|
| **一致性非锁定读** | 默认 — 使用 MVCC 快照 |
| **锁定读** | `SELECT ... FOR UPDATE` — 排他锁 |
| **共享锁读** | `SELECT ... LOCK IN SHARE MODE` — 共享锁 |
| **SKIP LOCKED** | 跳过被锁的行 |
| **NOWAIT** | 不等待锁 — 立即报错 |

### row_search_mvcc() 关键步骤

| 步骤 | 说明 |
|------|------|
| 1. 打开 B-Tree 游标 | 定位到聚簇索引 |
| 2. 搜索记录 | 从根到叶节点 |
| 3. MVCC 检查 | 检查 DB_TRX_ID 和 ReadView |
| 4. 版本链遍历 | 沿 DB_ROLL_PTR 查找可见版本 |
| 5. 二级索引回表 | 二级索引 → 聚簇索引 |
| 6. 返回行 | 转换为 MySQL 行格式 |

**Sources** · [storage/innobase/row/row0sel.cc](file://storage/innobase/row/row0sel.cc)

## 行插入（INSERT）

`row0ins.cc`（~120KB）实现行插入操作：

### 插入流程

```mermaid
graph TB
    INSERT[INSERT 语句]
    INSERT --> ROW_INS[row_ins_clust_index_entry]
    ROW_INS --> DUP_CHECK{唯一键检查}
    DUP_CHECK --> |有重复| DUP_ERROR[唯一键冲突]
    DUP_CHECK --> |无重复| BTR_INSERT[B-Tree 插入]
    BTR_INSERT --> SPLIT{页满?}
    SPLIT --> |是| PAGE_SPLIT[页分裂]
    SPLIT --> |否| INSERT_RECORD[插入记录]
    PAGE_SPLIT --> INSERT_RECORD
    INSERT_RECORD --> SECONDARY[更新二级索引]
    SECONDARY --> UNDO_LOG[写入 undo 日志]
    UNDO_LOG --> DONE[插入完成]
```

### 插入优化

| 优化 | 说明 |
|------|------|
| **顺序插入** | AUTO_INCREMENT 主键保证顺序插入 |
| **批量插入** | `LOAD DATA` 使用批量 B-Tree 加载 |
| **唯一检查** | 延迟唯一检查 — 批量验证 |
| **外键检查** | 按需触发外键验证 |

**Sources** · [storage/innobase/row/row0ins.cc](file://storage/innobase/row/row0ins.cc)

## 行更新（UPDATE/DELETE）

`row0upd.cc`（~109KB）实现行更新和删除操作：

### 更新流程

```mermaid
graph TB
    UPDATE[UPDATE 语句]
    UPDATE --> ROW_UPD[row_upd]
    ROW_UPD --> SEARCH[定位目标行]
    SEARCH --> UNDO[写入 undo 记录]
    UNDO --> CHECK{主键变更?}
    CHECK --> |是| DELETE_INSERT[删除旧行 + 插入新行]
    CHECK --> |否| IN_PLACE[原地更新]
    IN_PLACE --> UPDATE_INDEX[更新二级索引]
    DELETE_INSERT --> UPDATE_INDEX
    UPDATE_INDEX --> DONE[更新完成]
```

### DELETE 实现

DELETE 在 InnoDB 中是标记删除：

```mermaid
graph TB
    DELETE[DELETE 语句]
    DELETE --> MARK[标记删除 — 设置删除标志]
    MARK --> COMMIT{事务提交?}
    COMMIT --> |是| PURGE[Purge 线程物理删除]
    COMMIT --> |否| UNDO_DELETE[回滚 — 清除删除标志]
```

**Sources** · [storage/innobase/row/row0upd.cc](file://storage/innobase/row/row0upd.cc)

## MySQL 接口层

`row0mysql.cc`（~147KB）是 InnoDB 与 MySQL SQL 层之间的桥梁：

### 关键接口函数

| 函数 | 说明 |
|------|------|
| `row_mysql_insert_row()` | MySQL INSERT → InnoDB 插入 |
| `row_mysql_update_row()` | MySQL UPDATE → InnoDB 更新 |
| `row_mysql_delete_row()` | MySQL DELETE → InnoDB 删除 |
| `row_search_for_mysql()` | MySQL SELECT → InnoDB 搜索 |
| `row_create_table_for_mysql()` | MySQL CREATE TABLE → InnoDB 建表 |
| `row_drop_table_for_mysql()` | MySQL DROP TABLE → InnoDB 删表 |
| `row_rename_table_for_mysql()` | MySQL RENAME TABLE → InnoDB 重命名 |

### 行格式转换

```mermaid
graph TB
    MYSQL_ROW[MySQL 行格式]
    MYSQL_ROW --> CONVERT[格式转换 — row0mysql.cc]
    CONVERT --> INNODB_ROW[InnoDB 内部行格式]
    INNODB_ROW --> |包含| HIDDEN[隐藏列 — DB_TRX_ID + DB_ROLL_PTR]
    INNODB_ROW --> |包含| USER_DATA[用户数据列]
```

**Sources** · [storage/innobase/row/row0mysql.cc](file://storage/innobase/row/row0mysql.cc)

## 表导入导出

`row0import.cc`（~154KB）实现表空间的导入/导出功能：

### DISCARD/IMPORT 流程

```mermaid
graph TB
    EXPORT[ALTER TABLE t DISCARD TABLESPACE]
    EXPORT --> REMOVE[删除 .ibd 文件]
    EXPORT --> COPY[复制 .ibd 文件到目标]
    COPY --> IMPORT[ALTER TABLE t IMPORT TABLESPACE]
    IMPORT --> VALIDATE[验证表空间]
    VALIDATE --> ADAPT[适配 — 重建索引]
    ADAPT --> READY[导入完成]
```

```sql
-- 表空间导出/导入
ALTER TABLE orders DISCARD TABLESPACE;
-- 复制 orders.ibd 到目标服务器
ALTER TABLE orders IMPORT TABLESPACE;
```

**Sources** · [storage/innobase/row/row0import.cc](file://storage/innobase/row/row0import.cc)

## MVCC 版本链

`row0vers.cc`（~56KB）实现 MVCC 版本链的遍历：

### 版本链遍历

```mermaid
graph TB
    CURRENT[当前行 — DB_TRX_ID = TX100]
    CURRENT --> |DB_ROLL_PTR| UNDO1[Undo 版本1 — DB_TRX_ID = TX80]
    UNDO1 --> |DB_ROLL_PTR| UNDO2[Undo 版本2 — DB_TRX_ID = TX50]
    UNDO2 --> |DB_ROLL_PTR| OLDEST[最早版本 — DB_TRX_ID = TX10]

    READVIEW[ReadView — 活跃事务: TX60, TX90]
    READVIEW --> |TX100 活跃| SKIP1[跳过当前版本]
    READVIEW --> |TX80 已提交| VISIBLE1[可见 — 返回此版本]
```

**Sources** · [storage/innobase/row/row0vers.cc](file://storage/innobase/row/row0vers.cc)

## 在线 DDL 日志

`row0log.cc`（~127KB）实现在线 DDL 期间的并发 DML 日志：

### 在线 DDL 并发

```mermaid
graph TB
    DDL[在线 ALTER TABLE]
    DDL --> COPY_PHASE[拷贝阶段]
    COPY_PHASE --> LOG[DML 操作记录到临时日志]
    LOG --> APPLY_LOG[拷贝完成后应用日志]
    APPLY_LOG --> SWAP[切换新旧表]
    SWAP --> DONE[DDL 完成]
```

```sql
-- 在线 DDL — 不阻塞 DML
ALTER TABLE orders ADD INDEX idx_date (order_date),
                   ALGORITHM=INPLACE, LOCK=NONE;
```

**Sources** · [storage/innobase/row/row0log.cc](file://storage/innobase/row/row0log.cc)
