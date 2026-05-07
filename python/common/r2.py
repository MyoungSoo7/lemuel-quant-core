"""Cloudflare R2 (S3-compatible) 클라이언트 래퍼."""
from __future__ import annotations

import io
import os
from dataclasses import dataclass
from typing import Iterator

import boto3
import pandas as pd
import pyarrow.parquet as pq


@dataclass
class R2Config:
    endpoint: str
    bucket: str
    access_key: str
    secret_key: str
    region: str = "auto"

    @classmethod
    def from_env(cls) -> "R2Config":
        return cls(
            endpoint=os.environ["R2_ENDPOINT"],
            bucket=os.environ.get("R2_BUCKET", "lemuel-quant"),
            access_key=os.environ["R2_ACCESS_KEY"],
            secret_key=os.environ["R2_SECRET_KEY"],
        )


class R2Client:
    """R2 위에 올라간 data-warehouse Parquet 스냅샷을 읽기 위한 헬퍼."""

    def __init__(self, cfg: R2Config | None = None):
        self.cfg = cfg or R2Config.from_env()
        self._s3 = boto3.client(
            "s3",
            endpoint_url=self.cfg.endpoint,
            region_name=self.cfg.region,
            aws_access_key_id=self.cfg.access_key,
            aws_secret_access_key=self.cfg.secret_key,
        )

    def list_keys(self, prefix: str = "snapshots/") -> list[str]:
        keys: list[str] = []
        token: str | None = None
        while True:
            kwargs = {"Bucket": self.cfg.bucket, "Prefix": prefix}
            if token:
                kwargs["ContinuationToken"] = token
            resp = self._s3.list_objects_v2(**kwargs)
            for obj in resp.get("Contents", []):
                keys.append(obj["Key"])
            if not resp.get("IsTruncated"):
                break
            token = resp.get("NextContinuationToken")
        return keys

    def read_parquet(self, key: str) -> pd.DataFrame:
        body = self._s3.get_object(Bucket=self.cfg.bucket, Key=key)["Body"].read()
        return pq.read_table(io.BytesIO(body)).to_pandas()

    def read_many(self, keys: list[str]) -> Iterator[pd.DataFrame]:
        for k in keys:
            yield self.read_parquet(k)
