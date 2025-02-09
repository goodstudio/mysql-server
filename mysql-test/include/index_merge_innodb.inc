# Skiping this test from Valgrind execution as per Bug-14627884
--source include/not_valgrind.inc
# Adding big test option for this test.
--source include/big_test.inc

--source include/elide_costs.inc

# t/index_merge_innodb.test
#
# Index merge tests
#
# Last update:
# 2006-08-07 ML test refactored (MySQL 5.1)
#               Main code of several index_merge tests
#                            -> include/index_merge*.inc
#               wrapper t/index_merge_innodb.test sources now several 
#               include/index_merge*.inc files
#

let $engine_type= InnoDB;
# According to Oracle: "InnoDB's estimate for the index cardinality
# depends on a pseudo random number generator (it picks up random
# pages to sample). After an optimization that was made in r2625 two
# EXPLAINs started returning a different number of rows (3 instead of
# 4)".
# Since hypergraph does not support traditional EXPLAIN format,
# we now use tree format for all the plans. Which ever plan has unstable
# rows and therefore costs, we do not display "rows/costs" as part of the
# plan.
# InnoDB does not support Merge tables (affects include/index_merge1.inc)
let $merge_table_support= 0;

--source include/index_merge1.inc
--source include/index_merge_ror.inc
--source include/index_merge2.inc

--source include/index_merge_2sweeps.inc
--source include/index_merge_ror_cpk.inc


--echo #
--echo # Bug#11747423 32254: INDEX MERGE USED UNNECESSARILY
--echo #
CREATE TABLE t1 (
  id INT NOT NULL PRIMARY KEY,
  id2 INT NOT NULL,
  id3 INT NOT NULL,
  KEY (id2),
  KEY (id3),
  KEY covering_index (id2,id3)
) ENGINE=InnoDB;

INSERT INTO t1 VALUES (0, 0, 0), (1, 1, 1), (2, 2, 2), (3, 3, 3), (4, 4, 4), (5, 5, 5), (6, 6, 6), (7, 7, 7);
INSERT INTO t1 SELECT id + 8, id2 + 8, id3 +8 FROM t1;
INSERT INTO t1 SELECT id + 16, 7, 0 FROM t1;

-- disable_query_log
-- disable_result_log
analyze table t1;
-- enable_result_log
-- enable_query_log

--replace_regex $elide_costs
EXPLAIN SELECT count(*) FROM t1 WHERE id2=7 AND id3=0;

DROP TABLE t1;

--echo #
--echo # Bug#21697002 ASSERTION `KEYS >= 0.0' FAILED
--echo #

CREATE TABLE t1 (
  c1 VARCHAR(2),
  d1 DATETIME NOT NULL
);

CREATE TABLE t2 (
  pk INTEGER NOT NULL,
  c1 VARCHAR(2),
  c2 VARCHAR(2),
  PRIMARY KEY (pk),
  KEY (c1),
  KEY (c2)
) PARTITION BY KEY(pk) PARTITIONS 2;

SELECT c1
FROM t1
WHERE c1 IN
(
  SELECT t2.c1
  FROM t2
  WHERE t2.c1 = 'f'
     OR t2.c2 < 'y'
)
AND t1.d1 IS NULL;

DROP TABLE t1, t2;
