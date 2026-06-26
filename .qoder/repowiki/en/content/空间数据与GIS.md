
<cite>
**引用文件**
- [sql/gis/](file://sql/gis/)
- [sql/gis/srs/](file://sql/gis/srs/)
- [storage/innobase/gis/](file://storage/innobase/gis/)
- [mysql-test/suite/gis/](file://mysql-test/suite/gis/)
</cite>

## 目录
1. [GIS 子系统概述](#gis-子系统概述)
2. [空间数据类型](#空间数据类型)
3. [空间函数库](#空间函数库)
4. [空间参考系统（SRS）](#空间参考系统srs)
5. [空间索引](#空间索引)
6. [WKB/WKT 格式](#wkbwkt-格式)

## GIS 子系统概述

MySQL 提供了完整的空间数据支持，实现在 `sql/gis/` 目录中（99 个源文件）。它支持 Open Geospatial Consortium（OGC）标准的空间数据类型、空间函数和空间索引，使 MySQL 能够存储、查询和分析地理空间数据。

```mermaid
graph TB
    subgraph SQL 层 — sql/gis/
        GEOM[几何类型定义]
        FUNCTOR[空间操作 Functor]
        SRS[SRS — 空间参考系统]
        WKB[WKB/WKT 编解码]
    end

    subgraph 存储引擎层
        INNODB_GIS[storage/innobase/gis/ — R-tree 索引]
        MYISAM_RTREE[MyISAM — R-tree 索引]
    end

    GEOM --> FUNCTOR
    FUNCTOR --> SRS
    WKB --> GEOM
    GEOM --> INNODB_GIS
    GEOM --> MYISAM_RTREE
```

**Sources** · [sql/gis/](file://sql/gis/)

## 空间数据类型

MySQL 支持以下 OGC 标准空间数据类型：

| 类型 | 说明 | 示例 |
|------|------|------|
| **GEOMETRY** | 抽象基类型 | — |
| **POINT** | 二维点 | `POINT(1 1)` |
| **LINESTRING** | 线段 | `LINESTRING(0 0, 1 1, 2 2)` |
| **POLYGON** | 多边形 | `POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))` |
| **MULTIPOINT** | 多点集合 | `MULTIPOINT(0 0, 1 1)` |
| **MULTILINESTRING** | 多线段集合 | — |
| **MULTIPOLYGON** | 多多边形集合 | — |
| **GEOMETRYCOLLECTION** | 几何集合 | — |

```sql
-- 创建空间表
CREATE TABLE locations (
    id INT PRIMARY KEY,
    name VARCHAR(100),
    location POINT NOT NULL SRID 4326,
    boundary POLYGON SRID 4326,
    SPATIAL INDEX(location)
);

-- 插入空间数据
INSERT INTO locations (id, name, location)
VALUES (1, 'Beijing', ST_GeomFromText('POINT(116.4 39.9)', 4326));
```

**Sources** · [sql/gis/geometries.cc](file://sql/gis/geometries.cc)

## 空间函数库

`sql/gis/` 实现了丰富的空间操作函数，采用 Functor 模式设计：

### 空间关系函数

| 函数 | 源文件 | 说明 |
|------|--------|------|
| `ST_Contains()` | `covered_by.cc` | 一个几何体是否完全包含另一个 |
| `ST_Crosses()` | `crosses.cc` (~32KB) | 两个几何体是否交叉 |
| `ST_Disjoint()` | `disjoint.cc` (~24KB) | 两个几何体是否不相交 |
| `ST_Equals()` | `equals.cc` (~27KB) | 两个几何体是否相等 |
| `ST_Intersects()` | `intersects.cc` | 两个几何体是否相交 |
| `ST_Overlaps()` | `overlaps.cc` | 两个几何体是否重叠 |
| `ST_Touches()` | `touches.cc` | 两个几何体是否接触 |
| `ST_Within()` | `within.cc` | 一个几何体是否在另一个内部 |
| `ST_CoveredBy()` | `covered_by.cc` | 一个几何体是否被另一个覆盖 |

### 空间度量函数

| 函数 | 源文件 | 说明 |
|------|--------|------|
| `ST_Area()` | `area.cc` | 计算面积 |
| `ST_Length()` | `length.cc` | 计算线段长度 |
| `ST_Distance()` | `distance.cc` / `distance_functor.cc` (~22KB) | 计算两点距离 |
| `ST_Distance_Sphere()` | `distance_sphere.cc` | 球面距离（大圆距离） |
| `ST_Hausdorff_Distance()` | `hausdorff_distance.cc` | Hausdorff 距离 |
| `ST_Frechet_Distance()` | `frechet_distance.cc` | Fréchet 距离 |

### 空间运算函数

| 函数 | 源文件 | 说明 |
|------|--------|------|
| `ST_Buffer()` | `buffer.cc` (~14KB) | 创建缓冲区 |
| `ST_Intersection()` | `intersection.cc` / `intersection_functor.cc` | 求交集 |
| `ST_Union()` | `union.cc` / `union_functor.cc` | 求并集 |
| `ST_Difference()` | `difference.cc` / `difference_functor.cc` (~36KB) | 求差集 |
| `ST_SymDifference()` | `symdifference.cc` | 对称差集 |
| `ST_Transform()` | `transform.cc` | 坐标系变换 |
| `ST_Simplify()` | `simplify.cc` | 简化几何体 |
| `ST_IsValid()` | `is_valid.cc` | 验证几何体有效性 |
| `ST_IsSimple()` | `is_simple.cc` | 检查几何体简单性 |

**Sources** · [sql/gis/](file://sql/gis/)

## 空间参考系统（SRS）

`sql/gis/srs/` 实现空间参考系统管理：

| 文件 | 说明 |
|------|------|
| `srs.cc` / `srs.h` | SRS 定义和管理 |
| `wkt_parser.cc` / `wkt_parser.h` | WKT 格式的 SRS 定义解析 |

### SRID 管理

```sql
-- 查看可用 SRS
SELECT * FROM information_schema.st_spatial_reference_systems LIMIT 10;

-- 创建自定义 SRS
CREATE SPATIAL REFERENCE SYSTEM 12345
  SET ORGANIZATION 'Custom' SET OID 12345
  SET DEFINITION 'GEOGCS["Custom",DATUM["WGS_1984",...]]';
```

常用 SRID：
- **SRID 0** — 笛卡尔坐标系（无单位）
- **SRID 4326** — WGS 84（GPS 使用的经纬度坐标系）

**Sources** · [sql/gis/srs/](file://sql/gis/srs/)

## 空间索引

MySQL 支持 R-tree 空间索引，实现在 `storage/innobase/gis/` 和 `sql/gis/rtree_support.cc` 中：

### R-tree 索引

- 多维索引结构，适合空间范围查询
- 使用最小外包矩形（MBR）组织空间对象
- InnoDB 和 MyISAM 均支持
- 支持 `ST_Contains`、`ST_Intersects`、`ST_Within` 等关系查询的索引加速

```sql
-- 创建空间索引
CREATE SPATIAL INDEX idx_location ON locations(location);

-- 使用空间索引的查询
SELECT * FROM locations
WHERE ST_Contains(
    ST_GeomFromText('POLYGON((116 39, 117 39, 117 40, 116 40, 116 39))', 4326),
    location
);
```

### R-tree 支持文件

| 文件 | 说明 |
|------|------|
| `rtree_support.cc` | R-tree 操作支持 |
| `box.h` / `box_traits.h` | 最小外包矩形（MBR）定义 |
| `mbr_utils.cc` | MBR 工具函数 |

**Sources** · [storage/innobase/gis/](file://storage/innobase/gis/) · [sql/gis/rtree_support.cc](file://sql/gis/rtree_support.cc)

## WKB/WKT 格式

空间数据使用两种标准格式存储和传输：

| 格式 | 全称 | 说明 |
|------|------|------|
| **WKT** | Well-Known Text | 人类可读的文本格式 |
| **WKB** | Well-Known Binary | 紧凑的二进制格式 |

```sql
-- WKT 格式
SELECT ST_AsText(location) FROM locations;
-- 输出: POINT(116.4 39.9)

-- WKB 格式（内部存储格式）
SELECT ST_AsBinary(location) FROM locations;

-- 从 WKT 创建几何体
SELECT ST_GeomFromText('POINT(116.4 39.9)', 4326);

-- 从 WKB 创建几何体
SELECT ST_GeomFromWKB(binary_data, 4326);
```

| 文件 | 说明 |
|------|------|
| `wkb.cc` | WKB 编解码实现 |
| `wkb_visitor.cc` | WKB 序列化访问器 |
| `wkb_size_visitor.cc` | WKB 大小计算 |

**Sources** · [sql/gis/wkb.cc](file://sql/gis/wkb.cc)
