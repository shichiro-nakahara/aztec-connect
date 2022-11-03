output "cloudfront" {
  value = "${aws_cloudfront_distribution.wallet_distribution.id}"
}

output "s3" {
  value = "${aws_s3_bucket.wallet.bucket}"
}
