using Distributions

"""
    black_scholes(S, K, r, σ, T; q=0, kind=:call)

European option price. `S` spot, `K` strike, `r` risk-free, `σ` annual vol,
`T` years, `q` continuous dividend yield. `kind` :call or :put.
"""
function black_scholes(S, K, r, σ, T; q=0.0, kind::Symbol=:call)
    d1 = (log(S / K) + (r - q + 0.5σ^2) * T) / (σ * sqrt(T))
    d2 = d1 - σ * sqrt(T)
    N = Distributions.cdf
    nd = Distributions.Normal()
    if kind === :call
        return S * exp(-q * T) * N(nd, d1) - K * exp(-r * T) * N(nd, d2)
    elseif kind === :put
        return K * exp(-r * T) * N(nd, -d2) - S * exp(-q * T) * N(nd, -d1)
    else
        throw(ArgumentError("kind must be :call or :put"))
    end
end

"""
    greeks(S, K, r, σ, T; q=0, kind=:call)

Returns `(delta, gamma, vega, theta, rho)` in per-year units.
"""
function greeks(S, K, r, σ, T; q=0.0, kind::Symbol=:call)
    d1 = (log(S / K) + (r - q + 0.5σ^2) * T) / (σ * sqrt(T))
    d2 = d1 - σ * sqrt(T)
    nd = Distributions.Normal()
    pdf = Distributions.pdf(nd, d1)
    cdf = Distributions.cdf
    sign_call = kind === :call ? 1 : -1
    delta = sign_call * exp(-q * T) * cdf(nd, sign_call * d1)
    gamma = exp(-q * T) * pdf / (S * σ * sqrt(T))
    vega  = S * exp(-q * T) * pdf * sqrt(T)
    theta = -S * exp(-q * T) * pdf * σ / (2 * sqrt(T)) -
            sign_call * r * K * exp(-r * T) * cdf(nd, sign_call * d2) +
            sign_call * q * S * exp(-q * T) * cdf(nd, sign_call * d1)
    rho   = sign_call * K * T * exp(-r * T) * cdf(nd, sign_call * d2)
    return (; delta, gamma, vega, theta, rho)
end

"""
    implied_vol(price, S, K, r, T; q=0, kind=:call, init=0.3, tol=1e-6)

Newton-Raphson solver. Returns σ s.t. `black_scholes(S, K, r, σ, T) ≈ price`.
"""
function implied_vol(price, S, K, r, T; q=0.0, kind::Symbol=:call,
                     init=0.3, tol=1e-6, maxiter=100)
    σ = init
    for _ in 1:maxiter
        bs = black_scholes(S, K, r, σ, T; q, kind)
        v  = greeks(S, K, r, σ, T; q, kind).vega
        if v < 1e-12
            return σ
        end
        diff = bs - price
        if abs(diff) < tol
            return σ
        end
        σ = σ - diff / v
        σ = clamp(σ, 1e-6, 5.0)
    end
    return σ
end
